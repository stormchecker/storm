#pragma once

#include <bit>
#include <bitset>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <ranges>
#include <span>

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/umb/model/StringEncoding.h"
#include "storm/storage/umb/model/ValuationDescription.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/utility/bitoperations.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm::umb {

class Valuations {
   public:
    using Integer = storm::NumberTraits<storm::RationalNumber>::IntegerType;

    Valuations(Valuations const&) = default;
    Valuations(Valuations&&) = default;
    Valuations& operator=(Valuations const&) = default;
    Valuations& operator=(Valuations&&) = default;

    Valuations(uint64_t const numEntities, std::vector<ValuationClassDescription> const& descriptions, std::vector<char> valuations,
               std::vector<uint64_t> stringMapping, std::vector<char> strings, std::optional<std::vector<uint32_t>> classes = {},
               std::vector<std::shared_ptr<storm::expressions::ExpressionManager const>> expressionManagers = {})
        : numEntities(numEntities), valuations(std::move(valuations)), stringMapping(std::move(stringMapping)), strings(std::move(strings)) {
        STORM_LOG_ASSERT(descriptions.size() == expressionManagers.size() || expressionManagers.size() <= 1,
                         "Mismatch between number of descriptions and expression managers.");
        // We either have a separate manager for each class or all classes share the same manager.
        // Furthermore, we might create a new manager for a class, if no manager was given explicitly.
        auto sharedManager = std::make_shared<storm::expressions::ExpressionManager>();
        for (uint64_t i = 0; i < descriptions.size(); ++i) {
            if (expressionManagers.empty() || (expressionManagers.size() == 1 && expressionManagers.front() == nullptr)) {
                // Shared manager for all classes, not given explicitly
                variableClasses.push_back(createVariablesInformation(*sharedManager, descriptions[i]));
            } else if (expressionManagers.size() == 1) {
                // Shared manager for all classes, given explicitly
                variableClasses.push_back(createVariablesInformation(*expressionManagers.front(), descriptions[i]));
            } else if (expressionManagers[i] == nullptr) {
                // Separate manager for each class, not given explicitly
                auto manager = std::make_shared<storm::expressions::ExpressionManager>();
                variableClasses.push_back(createVariablesInformation(*manager, descriptions[i]));
            } else {
                // Separate manager for each class, given explicitly
                variableClasses.push_back(createVariablesInformation(*expressionManagers[i], descriptions[i]));
            }
        }
        if (classes.has_value() && this->variableClasses.size() > 1) {
            STORM_LOG_ASSERT(numEntities == classes->size(), "Number of entities does not match class mapping size.");
            this->entityClassMappings = {std::move(*classes), std::vector<uint64_t>{1, 0}};
            uint64_t pos = 0;
            this->entityClassMappings->toValuationsMapping.reserve(this->entityClassMappings->toClassMapping.size() + 1);
            for (uint64_t entity = 0; entity < this->entityClassMappings->toClassMapping.size(); ++entity) {
                STORM_LOG_ASSERT(
                    this->entityClassMappings->toClassMapping[entity] < this->variableClasses.size(),
                    "Class index " << this->entityClassMappings->toClassMapping[entity] << " out of bounds. Only " << descriptions.size() << "classes known.");
                pos += this->variableClasses[this->entityClassMappings->toClassMapping[entity]].sizeInBytes;
                this->entityClassMappings->toValuationsMapping.push_back(pos);
            }
            STORM_LOG_ASSERT(this->valuations.size() == pos, "Valuation data size does not match class mapping.");
        } else {
            STORM_LOG_ASSERT(this->variableClasses.size() == 1, "Valuation descriptions must be unique if no class mapping is given.");
            STORM_LOG_ASSERT(!classes.has_value() || std::all_of(classes->begin(), classes->end(), [&](auto classIndex) { return classIndex == 0; }),
                             "A single description is given but the class mapping is not unique.");
            STORM_LOG_ASSERT(this->variableClasses.front().sizeInBytes == 0 || valuations.size() % this->variableClasses.front().sizeInBytes == 0,
                             "Valuation data size is not a multiple of the unique valuation size.");
            STORM_LOG_ASSERT(numEntities * this->variableClasses.front().sizeInBytes == valuations.size(),
                             "Valuation data size does not match number of entities.");
        }
    }

    Valuations(uint64_t const numEntities, ValuationClassDescription const& description, std::vector<char> valuations,
               std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager = {})
        : Valuations(numEntities, {description}, std::move(valuations), {}, {}, std::nullopt, {expressionManager}) {
        // Intentionally empty
    }

    Valuations(ValuationClassDescription const& description, std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager = {})
        : Valuations(0, description, {}, expressionManager) {
        // Intentionally empty
    }

    /*!
     * @return the number of entities (e.g. states) that this valuation assigns values for
     */
    uint64_t size() const {
        return numEntities;
    }

    uint64_t numClasses() const {
        return variableClasses.size();
    }

    ValuationClassDescription getClassDescription(uint64_t classIndex = 0) const {
        STORM_LOG_ASSERT(classIndex < numClasses(), "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
        ValuationClassDescription res;
        uint64_t currBit = 0;
        for (auto const& varInfo : variableClasses[classIndex].variables) {
            if (uint64_t padding = varInfo.bitOffset - currBit; padding > 0) {
                res.variables.push_back(storm::umb::ValuationClassDescription::Padding(padding));
            }
            res.variables.push_back(varInfo.description);
            currBit = varInfo.bitOffset + varInfo.description.type.bitSize();
        }
        if (uint64_t padding = currBit % 8; padding > 0) {
            res.variables.push_back(storm::umb::ValuationClassDescription::Padding(8 - padding));
        }
        return res;
    }

    std::span<char const> getRawBytes(uint64_t entity) const {
        STORM_LOG_ASSERT(entity < size(), "Entity index out of bounds: " << entity << " >= " << size() << ".");
        if (entityClassMappings) {
            auto const start = entityClassMappings->toValuationsMapping[entity];
            auto const end = entityClassMappings->toValuationsMapping[entity + 1];
            return std::span<char const>(&valuations[start], end - start);
        } else {
            auto const start = entity * variableClasses.front().sizeInBytes;
            return std::span<char const>(&valuations[start], variableClasses.front().sizeInBytes);
        }
    }

    std::span<char> getRawBytes(uint64_t entity) {
        if (entityClassMappings) {
            auto const start = entityClassMappings->toValuationsMapping[entity];
            auto const end = entityClassMappings->toValuationsMapping[entity + 1];
            return std::span<char>(&valuations[start], end - start);
        } else {
            auto const start = entity * variableClasses.front().sizeInBytes;
            return std::span<char>(&valuations[start], variableClasses.front().sizeInBytes);
        }
    }

    uint64_t numStrings() const {
        return stringMapping.size() > 0 ? stringMapping.size() - 1 : 0;
    }

    void resize(uint64_t newEntityCount, uint64_t const classIndex = 0) {
        if (newEntityCount > size()) {
            // Initialize one new entity with default values. This is required to ensure that valuation data is consistent (e.g. avoid 0/0 for rationals).
            emplaceBack<true>(classIndex, [](auto&&...) {});
            // For the remaining entities, we can be a bit quicker by copying the values of the last initialized entity.
            if (newEntityCount > size()) {
                uint64_t const classSize = variableClasses[classIndex].sizeInBytes;
                valuations.resize(valuations.size() + (newEntityCount - size()) * classSize);
                if (entityClassMappings) {
                    entityClassMappings->toClassMapping.resize(newEntityCount, classIndex);
                    for (uint64_t valEnd = entityClassMappings->toValuationsMapping.back() + classSize; valEnd < valuations.size(); valEnd += classSize) {
                        entityClassMappings->toValuationsMapping.push_back(valEnd);
                    }
                }
                auto const srcBytes = getRawBytes(numEntities);  // the bytes of the entry we initialized using emplaceBack
                for (uint64_t newEntityIndex = numEntities + 1; newEntityIndex < newEntityCount; ++newEntityIndex) {
                    auto destBytes = getRawBytes(newEntityIndex);
                    std::copy(srcBytes.begin(), srcBytes.end(), destBytes.begin());
                }
                numEntities = newEntityCount;
            }
        } else if (newEntityCount < size()) {
            uint64_t const newValuationsSize = entityClassMappings.has_value() ? entityClassMappings->toValuationsMapping[newEntityCount]
                                                                               : newEntityCount * variableClasses.front().sizeInBytes;
            valuations.resize(newValuationsSize);
            if (entityClassMappings) {
                entityClassMappings->toClassMapping.resize(newEntityCount);
                entityClassMappings->toValuationsMapping.resize(newEntityCount + 1);
            }
            numEntities = newEntityCount;
        }
    }

    storm::expressions::ExpressionManager const& getManager() const {
        auto const& manager = variableClasses.front().expressionManager;
        STORM_LOG_THROW(
            std::all_of(variableClasses.begin(), variableClasses.end(), [&manager](auto const& varClass) { return varClass.expressionManager == manager; }),
            storm::exceptions::IllegalFunctionCallException, "Expression manager is not unique.");
        return *manager;
    }

    storm::expressions::ExpressionManager const& getManager(uint64_t classIndex) const {
        STORM_LOG_ASSERT(classIndex < variableClasses.size(),
                         "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
        return *variableClasses[classIndex].expressionManager;
    }

    template<bool AllowOptional = false, typename... AllowedTypes>
    void emplaceBack(uint64_t const classIndex, auto const& callback) {
        STORM_LOG_ASSERT(classIndex < variableClasses.size(),
                         "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
        // Enlarge the valuation data by one entry and initialize it with zeros.
        // This ensures a consistent state of valuation data, in particular padding bits will always be 0 this way.
        valuations.resize(valuations.size() + variableClasses[classIndex].sizeInBytes, 0);
        if (entityClassMappings) {
            entityClassMappings->toClassMapping.push_back(classIndex);
            entityClassMappings->toValuationsMapping.push_back(valuations.size());
        }
        ++numEntities;
        writeCallback<false, AllowOptional, AllowedTypes...>(size() - 1, callback);
    }

   private:
    struct VariableInformation;
    VariableInformation const& getVariableInformation(uint64_t entity, storm::expressions::Variable const& variable) const {
        auto const& vars = info(entity).variables;
        auto varInfoIt = std::find_if(vars.begin(), vars.end(), [&variable](auto const& varInfo) { return varInfo.expressionVariable == variable; });
        STORM_LOG_ASSERT(varInfoIt == vars.end(), "Can not find unknown variable " << variable.getName() << ".");
        return *varInfoIt;
    }

    VariableInformation const& getVariableInformation(storm::expressions::Variable const& variable) const {
        STORM_LOG_ASSERT(numClasses() == 1, "Trying to get variable information but the class is not unique among entities.");
        return getVariableInformation(0, variable);
    }

   public:
    template<bool AllowOptional = false, typename... AllowedTypes>
    void emplaceBack(auto const& callback) {
        STORM_LOG_ASSERT(variableClasses.size() == 1, "Trying to add a valuation but the class is not unique.");
        emplaceBack<AllowOptional, AllowedTypes...>(0, callback);
    }

    template<typename... AllowedTypes>
    void readCallback(uint64_t entity, auto const& callback) const {
        for (auto const& varInfo : info(entity).variables) {
            read<AllowedTypes...>(entity, varInfo, callback);
        }
    }

    template<typename... AllowedTypes>
    void readCallback(uint64_t entity, storm::expressions::Variable const& variable, auto const& callback) const {
        read<AllowedTypes...>(entity, getVariableInformation(entity, variable), callback);
    }

    template<typename ValueType>
    ValueType readValue(uint64_t entity, storm::expressions::Variable const& variable) const {
        ValueType result;
        readCallback<ValueType>(entity, variable, [&](auto&&... args, ValueType&& value) { result = std::move(value); });
        return result;
    }

    template<typename... AllowedTypes>
    void readCallback(storm::expressions::Variable const& variable, auto const& callback) const {
        if (numClasses() == 1) {
            // We have only one class, so we can look up the variable info once and use it for all entities
            auto const& varInfo = getVariableInformation(variable);
            for (uint64_t entity = 0; entity < size(); ++entity) {
                read<AllowedTypes...>(entity, varInfo, callback);
            }
        } else {
            // We have multiple classes, so we need to look up the variable info for each entity separately
            for (uint64_t entity = 0; entity < size(); ++entity) {
                readCallback<AllowedTypes...>(entity, variable, callback);
            }
        }
    }

    template<typename... AllowedTypes>
    void readCallback(auto const& callback) const {
        for (uint64_t entity = 0; entity < size(); ++entity) {
            readCallback<AllowedTypes...>(entity, callback);
        }
    }

    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes>
    void writeCallback(uint64_t entity, auto const& callback) {
        for (auto const& varInfo : info(entity).variables) {
            write<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, varInfo, callback);
        }
    }

    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes>
    void writeCallback(uint64_t entity, storm::expressions::Variable const& variable, auto const& callback) {
        write<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, getVariableInformation(entity, variable), callback);
    }

    template<typename ValueType>
    void writeValue(uint64_t entity, storm::expressions::Variable const& variable, ValueType const& value) {
        writeCallback<false, false, ValueType>(entity, variable, [&value](auto const& e, auto const& var, ValueType& val) { val = value; });
    }

    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes>
    void writeCallback(auto const& callback) {
        for (uint64_t entity = 0; entity < size(); ++entity) {
            writeCallback<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, callback);
        }
    }

    /*!
     * Constructs a new Valuations object containing only the selected entities in the given order.
     * @param selectedEntities
     * @return
     */
    Valuations selectEntities(auto const& selectedEntities) const {
        Valuations result(variableClasses);
        result.numEntities = [&selectedEntities]() {
            if constexpr (std::is_same_v<std::remove_cvref<decltype(selectedEntities)>, storm::storage::BitVector>) {
                return selectedEntities.getNumberOfSetBits();
            } else {
                return std::ranges::distance(selectedEntities);
            }
        }();
        result.stringMapping = stringMapping;
        result.strings = strings;

        if (entityClassMappings) {
            result.entityClassMappings.emplace();
            result.entityClassMappings->toValuationsMapping.reserve(result.numEntities + 1);
            result.entityClassMappings->toValuationsMapping.push_back(0);  // first entry of toValuationsMapping must be 0
            result.entityClassMappings->toClassMapping.reserve(result.numEntities);
        } else {
            result.valuations.reserve(result.numEntities * result.variableClasses.front().sizeInBytes);
        }
        for (auto const oldEntityIndex : selectedEntities) {
            STORM_LOG_ASSERT(oldEntityIndex < size(), "Selected entity index " << oldEntityIndex << " out of bounds. Only " << size() << " entities known.");
            auto const bytes = getRawBytes(oldEntityIndex);
            result.valuations.insert(valuations.end(), bytes.begin(), bytes.end());
            if (entityClassMappings) {
                result.entityClassMappings->toValuationsMapping.push_back(result.valuations.size());
                result.entityClassMappings->toClassMapping.push_back(entityClassMappings->toClassMapping[oldEntityIndex]);
            }
        }
        return result;
    }

   private:
    uint64_t numEntities;

    // Variable information
    struct VariableInformation {
        storm::expressions::Variable const expressionVariable;
        ValuationClassDescription::Variable const description;
        uint64_t const bitOffset;  // The first bit holding the variable's data within the valuation.
        // If the variable is optional, the optional bit is located at bitOffset - 1
        bool const fits64Bit;
    };
    struct VariablesInformation {
        std::vector<VariableInformation> const variables;
        std::shared_ptr<storm::expressions::ExpressionManager const> const expressionManager;
        uint64_t const sizeInBytes;
    };
    std::vector<VariablesInformation> variableClasses;

    // Classes information
    struct ClassData {
        std::vector<uint32_t> toClassMapping;       // mapping from entity index to class index (size equals number of entities)
        std::vector<uint64_t> toValuationsMapping;  // CSR mapping from entity index to valuations
    };
    std::optional<ClassData> entityClassMappings;  // present iff there are multiple classes

    // Data
    std::vector<char> valuations;
    std::vector<uint64_t> stringMapping;
    std::vector<char> strings;

    Valuations(std::vector<VariablesInformation> const& variableClasses) : variableClasses(variableClasses), numEntities(0) {
        // Intentionally empty
    }

    VariablesInformation const& info(uint64_t entity) const {
        STORM_LOG_ASSERT(entity < size(), "Entity index out of bounds: " << entity << " >= " << size() << ".");
        if (entityClassMappings) {
            return variableClasses[entityClassMappings->toClassMapping[entity]];
        } else {
            return variableClasses.front();
        }
    }

    /*!
     * Return true if the variable representation and potential offset addition all fit inside a standard 64 bit number representation
     * @return
     */
    static bool fits64Bit(ValuationClassDescription::Variable const& varDesc) {
        using enum storm::umb::Type;
        if (varDesc.type.bitSize() > 64) {
            return false;
        }
        switch (varDesc.type.type) {
            case Bool:
                return true;
            case Uint: {
                if (varDesc.offset.value_or(0) == 0) {
                    return true;
                }
                // check if adding the (non-zero) offset still fits into 64 bits
                uint64_t const maxValue = varDesc.upper.has_value()        ? static_cast<uint64_t>(varDesc.upper.value())
                                          : (varDesc.type.bitSize() == 64) ? std::numeric_limits<uint64_t>::max()
                                                                           : (static_cast<uint64_t>(1) << varDesc.type.bitSize()) - 1;
                // the minValue equals the offset (given as int64_t and therefore always fits into 64 bits
                if (varDesc.offset.value() < 0) {
                    // maxValue + offset = maxValue - (-offset) must fit into output type int64_t
                    return maxValue - static_cast<uint64_t>(-varDesc.offset.value()) <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
                } else {  // positive offset
                    // maxValue + offset must fit into output type uint64_t
                    return maxValue <= std::numeric_limits<uint64_t>::max() - static_cast<uint64_t>(varDesc.offset.value());
                }
            }
            case Int: {
                if (varDesc.offset.value_or(0) == 0) {
                    return true;
                }
                // check if adding the (non-zero) offset still fits into 64 bits
                int64_t const maxValue = varDesc.upper.value_or(varDesc.type.bitSize() == 64 ? std::numeric_limits<int64_t>::max()
                                                                                             : (static_cast<int64_t>(1) << (varDesc.type.bitSize() - 1)) - 1);
                int64_t const minValue = varDesc.lower.value_or(varDesc.type.bitSize() == 64 ? std::numeric_limits<int64_t>::min()
                                                                                             : -(static_cast<int64_t>(1) << (varDesc.type.bitSize() - 1)));
                if (varDesc.offset.value() < 0) {
                    // minValue + offset must fit into output type int64_t
                    return minValue >= std::numeric_limits<int64_t>::min() - varDesc.offset.value();
                } else {  // positive offset
                    // maxValue + offset must fit into output type int64_t
                    return maxValue <= std::numeric_limits<int64_t>::max() - varDesc.offset.value();
                }
            }
            case Double:
                return true;  // double values are always stored as 64 bit IEEE 754 values
            case Rational:
                return false;  //
            case String:
                return true;  // string indices are always stored as uint64_t
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Valuations for variable type '" << varDesc.type.toString() << "' are not supported.");
        }
    }

    static VariablesInformation createVariablesInformation(auto& expressionManager, ValuationClassDescription const& description) {
        std::vector<VariableInformation> variables;
        uint64_t currentOffset = 0;
        for (auto const& varVariant : description.variables) {
            if (std::holds_alternative<ValuationClassDescription::Variable>(varVariant)) {
                auto const& varDesc = std::get<ValuationClassDescription::Variable>(varVariant);
                storm::expressions::Variable exprVar;
                if constexpr (std::is_const_v<std::remove_reference_t<decltype(expressionManager)>>) {
                    exprVar = expressionManager.getVariable(varDesc.name);
                } else {
                    storm::expressions::Type variableType;
                    using enum storm::umb::Type;
                    switch (varDesc.type.type) {
                        case Bool:
                            variableType = expressionManager.getBooleanType();
                            break;
                        case Uint:
                        case Int:
                            variableType = expressionManager.getIntegerType();
                            break;
                        case Double:
                        case Rational:
                            variableType = expressionManager.getRationalType();
                            break;
                        case String:
                            variableType = expressionManager.getStringType();
                            break;
                        default:
                            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                            "Valuations for variable type '" << varDesc.type.toString() << "' are not supported.");
                    }
                    exprVar = expressionManager.declareOrGetVariable(varDesc.name, variableType);
                }
                if (varDesc.isOptional.value_or(false)) {
                    ++currentOffset;  // optional variables have a preceding presence bit
                }
                variables.emplace_back(
                    VariableInformation{.expressionVariable = exprVar, .description = varDesc, .bitOffset = currentOffset, .fits64Bit = fits64Bit(varDesc)});
                currentOffset += variables.back().description.type.bitSize();
            } else {
                auto const& padding = std::get<ValuationClassDescription::Padding>(varVariant);
                currentOffset += padding.padding;
            }
        }
        STORM_LOG_ASSERT(currentOffset == description.sizeInBits(), "Computed size does not match description size.");
        STORM_LOG_ASSERT(currentOffset % 8 == 0, "Invalid valuation description detected: size in bits must be a multiple of 8.");
        return VariablesInformation{
            .variables = std::move(variables), .expressionManager = expressionManager.shared_from_this(), .sizeInBytes = currentOffset / 8};
    }

    bool readBit(std::span<char const> bytes, uint64_t const position) const {
        STORM_LOG_ASSERT(position < bytes.size() * 8, "Bit position exceeds valuation size.");
        return bytes[position / 8] & (1 << (position % 8));
    }

    void writeBit(std::span<char> bytes, uint64_t const position, bool value) const {
        STORM_LOG_ASSERT(position < bytes.size() * 8, "Bit position exceeds valuation size.");
        char& byte = bytes[position / 8];
        char const pos = (1 << (position % 8));
        if (value) {
            byte |= pos;
        } else {
            byte &= ~pos;
        }
    }

    uint64_t readUint64(std::span<char const> bytes, uint64_t const bitOffset, uint64_t const bitSize) const {
        STORM_LOG_ASSERT(bitOffset < bytes.size() * 8, "Variable offset exceeds valuation size.");
        STORM_LOG_ASSERT(bitSize <= 64, "Invalid bit range.");
        auto const firstByte = bitOffset / 8;
        auto const bitOffsetWithinByte = bitOffset % 8;
        auto const numBytes = (bitOffsetWithinByte + bitSize + 7) / 8;
        STORM_LOG_ASSERT(numBytes <= 9, "Invalid number of bytes computed: " << numBytes);
        uint64_t result;
        // set the first (up to) 8 bytes
        std::memcpy(&result, &bytes[firstByte], std::min<uint64_t>(numBytes, 8ull));
        result >>= bitOffsetWithinByte;
        // if necessary, set the most significant bits by reading a 9th byte
        if (numBytes == 9ull) {
            uint64_t upperBits = std::bit_cast<uint8_t>(bytes[firstByte + 8]);
            upperBits <<= (64 - bitOffsetWithinByte);
            result |= upperBits;
        }
        // Set irrelevant bits to zero
        if (bitSize < 64) {
            uint64_t const relevantBitMask = (1ull << bitSize) - 1;
            result &= relevantBitMask;
        }
        return result;
    }

    void writeUint64(std::span<char> bytes, uint64_t const bitOffset, uint64_t const bitSize, uint64_t const value) const {
        STORM_LOG_ASSERT(bitOffset < bytes.size() * 8, "Variable offset exceeds valuation size.");
        STORM_LOG_ASSERT(bitSize <= 64, "Invalid bit range.");
        STORM_LOG_ASSERT(bitSize == 64 || value < (1ull << bitSize), "Invalid value " << value << " for bit size " << bitSize);
        uint64_t const firstByte = bitOffset / 8;
        uint8_t const bitOffsetWithinByte = bitOffset % 8;
        uint8_t const numBytes = (bitOffsetWithinByte + bitSize + 7) / 8;
        uint8_t const numFullBytes = (bitOffsetWithinByte + bitSize) / 8;
        STORM_LOG_ASSERT(numBytes <= 9, "Invalid number of bytes computed: " << numBytes);
        if (numFullBytes == 0) {
            // We only have to write into a single byte
            char& byte = bytes[firstByte];
            uint8_t const relevantBitsMask = ((1 << bitSize) - 1) << bitOffsetWithinByte;  // e.g. 0000 1110 for bitOffsetWithinByte=1 and bitSize=3
            byte &= static_cast<char>(~relevantBitsMask);                                  // set relevant bits to zero
            byte |= static_cast<char>((value << bitOffsetWithinByte) & relevantBitsMask);  // set relevant bits to the value bits
        } else {
            // First write all full bytes
            if (bitOffsetWithinByte == 0) {
                // Fast path: variable is byte-aligned, so we can directly write all full bytes without bit shifts
                std::memcpy(&bytes[firstByte], &value, numFullBytes);
            } else {
                uint64_t const shiftedValue = static_cast<uint64_t>(bytes[firstByte]) & ((1ull << bitOffsetWithinByte) - 1) | (value << bitOffsetWithinByte);
                std::memcpy(&bytes[firstByte], &shiftedValue, numFullBytes);
            }
            // Then write the last byte if necessary
            if (numFullBytes != numBytes) {
                // we have to write a partial byte at the end, so we need to read the existing byte and only overwrite the relevant bits
                char& lastByte = bytes[firstByte + numFullBytes];
                uint8_t const numBitsUsedInLastByte = (bitOffsetWithinByte + bitSize) % 8;
                lastByte &= static_cast<char>((1 << numBitsUsedInLastByte) - 1);                   // set relevant bits to zero
                lastByte |= static_cast<char>(value >> (numFullBytes * 8 - bitOffsetWithinByte));  // set relevant bits to the value bits
            }
        }
    }

    template<bool Signed>
    Integer readInteger(std::span<char const> bytes, uint64_t const bitOffset, uint64_t const bitSize) const {
        auto const num64BitChunks = (bitSize + 63) / 64;
        auto chunksView =
            std::ranges::iota_view(0ull, num64BitChunks) | std::ranges::views::transform([this, &bytes, &bitOffset, &bitSize](auto i) -> uint64_t {
                return readUint64(bytes, bitOffset + i * 64, std::min<uint64_t>(64, bitSize - i * 64));
            });
        Integer result = ValueEncoding::decodeArbitraryPrecisionInteger<false>(chunksView);
        if constexpr (Signed) {
            // Check if this number is supposed to be negative
            if (result >= storm::utility::pow<Integer>(2, bitSize - 1)) {
                return result - storm::utility::pow<Integer>(2, bitSize);
            }
        }
        return result;
    }

    /*!
     * Reads the given variable for the given entity and calls the callback with the read value.
     * @tparam AllowedTypes either empty (allowing all types) or a list of types that are handled in the callback. The following types are considered:
     * @param entity the entity (state/choice/branch/observation index)
     * @param varInfo The info for the given variable
     * @param callback The callback.
     * @param defaulOptional if true, optional variables that are not set will be given a default value
     */
    template<typename... AllowedTypes>
    void read(uint64_t entity, VariableInformation const& varInfo, auto const& callback) const {
        auto invokeCallback = [&entity, &varInfo, &callback](auto&& value) -> bool {
            bool constexpr IsAllowed = (sizeof...(AllowedTypes) == 0) || std::disjunction_v<std::is_same<std::remove_cvref<decltype(value)>, AllowedTypes>...>;
            if constexpr (IsAllowed) {
                callback(entity, varInfo.expressionVariable, value);
            }
            return IsAllowed;
        };

        if (varInfo.description.isOptional.value_or(false)) {
            STORM_LOG_ASSERT(varInfo.bitOffset > 0, "Invalid variable information: optional variable must have a preceding presence bit.");
            if (bool const hasValue = readBit(getRawBytes(entity), varInfo.bitOffset - 1); !hasValue) {
                if (invokeCallback(std::nullopt)) {
                    return;
                }
            }
        }
        auto const bitSize = varInfo.description.type.bitSize();
        using enum storm::umb::Type;
        if (varInfo.fits64Bit) {
            STORM_LOG_ASSERT(bitSize <= 64, "Invalid bit size for 64 bit fast path.");
            uint64_t const rawContent = readUint64(getRawBytes(entity), varInfo.bitOffset, bitSize);
            switch (varInfo.description.type.type) {
                case Bool:
                    if (invokeCallback(rawContent != 0)) {
                        return;
                    }
                    break;
                case Uint:
                    if (int64_t offset = varInfo.description.offset.value_or(0); offset < 0) {
                        // negative offset, output type is int64_t
                        if (invokeCallback(static_cast<int64_t>(rawContent) + offset)) {
                            return;
                        }
                    } else {
                        // non-negative offset, output type is uint64_t but we also try int64_t if uint64_t is not allowed
                        uint64_t const value = rawContent + offset;
                        uint64_t constexpr maxInt64 = std::numeric_limits<int64_t>::max();
                        if (invokeCallback(value) || (value <= maxInt64 && invokeCallback(static_cast<int64_t>(value)))) {
                            return;
                        }
                    }
                    break;
                case Int: {
                    uint64_t const mostSignificantBitMask = 1ull << (bitSize - 1);
                    bool const isNegative = rawContent & mostSignificantBitMask;
                    // For negative value, take the two's complement (e.g. 1111...1101 is -3)
                    int64_t const value =
                        isNegative ? (-static_cast<int64_t>(~rawContent & (mostSignificantBitMask - 1)) - 1) : static_cast<int64_t>(rawContent);
                    if (invokeCallback(value)) {
                        return;
                    }
                    break;
                }
                case Double:
                    if (invokeCallback(std::bit_cast<double>(rawContent))) {
                        return;
                    }
                    break;
                case Rational:
                    // Reaching this part should not be possible as varInfo.fits64Bit would be false
                    STORM_LOG_ASSERT(false, "Handling of rational values in 64 bit fast path is not implemented.");
                    break;
                case String:
                    STORM_LOG_ASSERT(rawContent < numStrings(), "String index " << rawContent << " out of bounds (> " << numStrings() << ").");
                    // Prefer the string_view callback
                    if (std::string_view const sv = stringVectorView(strings, stringMapping)[rawContent];
                        invokeCallback(sv) || invokeCallback(std::string(sv))) {
                        return;
                    }
                    break;
                default:
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                    "Valuations for variable type '" << varInfo.description.type.toString() << "' are not supported.");
            }
        }
        // reaching this point means that we could not handle the value in the fast path
        switch (varInfo.description.type.type) {
            case Bool:
                // Bools could be encoded with more than 64 bits (which is not reasonable, but possible..)
                if (invokeCallback(readInteger<false>(getRawBytes(entity), varInfo.bitOffset, bitSize) != Integer(0))) {
                    return;
                }
                break;
            case Uint:
            case Int: {
                Integer value = varInfo.description.type.type == Int ? readInteger<true>(getRawBytes(entity), varInfo.bitOffset, bitSize)
                                                                     : readInteger<false>(getRawBytes(entity), varInfo.bitOffset, bitSize);
                value += storm::utility::convertNumber<Integer>(varInfo.description.offset.value_or(0));
                if (invokeCallback(value)) {
                    return;
                }
                break;
            }
            case Double:
                // Reaching this part should not be possible as varInfo.fits64Bit would be true
                STORM_LOG_ASSERT(false, "double variables with more than 64 bits are not compliant.");
                break;
            case Rational: {
                STORM_LOG_ASSERT(bitSize % 2 == 0, "Rational number bit size must be even.");
                uint64_t const b = bitSize / 2;
                storm::RationalNumber const numerator = readInteger<true>(getRawBytes(entity), varInfo.bitOffset, b);
                storm::RationalNumber const denominator = readInteger<false>(getRawBytes(entity), varInfo.bitOffset + b, b);
                if (invokeCallback(storm::RationalNumber(numerator / denominator))) {
                    return;
                }
                break;
            }
            case String: {
                // Reaching this part should not be possible as varInfo.fits64Bit would be true
                STORM_LOG_ASSERT(false, "String variables with more than 64 bits are not compliant.");
                break;
            }
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Valuations for variable type '" << varInfo.description.type.toString() << "' are not supported.");
        }

        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                        "Variable " << varInfo.description.name << " of type " << varInfo.description.type.toString() << " is not handled.");
    }

    template<bool Signed>
    void writeInteger(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, Integer const& value) const {
        auto const num64BitChunks = (bitSize + 63) / 64;
        std::vector<uint64_t> uint64Encoding;
        ValueEncoding::appendEncodedInteger<Signed>(uint64Encoding, value, num64BitChunks);
        STORM_LOG_ASSERT(uint64Encoding.size() == num64BitChunks, "Encoding does not fit into the specified bit size.");
        for (auto const& v : uint64Encoding) {
            if (bitSize >= 64) {
                writeUint64(bytes, bitOffset, 64, v);
                bitOffset += 64;
                bitSize -= 64;
            } else {
                writeUint64(bytes, bitOffset, bitSize, v);
                bitSize = 0;
                break;
            }
        }
        STORM_LOG_ASSERT(bitSize == 0, "Unexpected integer encoding. Not all bits were written.");
    }

    template<typename ValueType>
    void writeValue(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, ValueType const& value) {
        if constexpr (std::is_same_v<ValueType, bool>) {
            writeUint64(bytes, bitOffset, bitSize, value ? 1ul : 0ul);
        } else if constexpr (std::is_same_v<ValueType, uint64_t>) {
            writeUint64(bytes, bitOffset, bitSize, value);
        } else if constexpr (std::is_same_v<ValueType, int64_t>) {
            if (value < 0) {
                // For negative value, take the two's complement (e.g. 1111...1101 is -3)
                uint64_t v = ~static_cast<uint64_t>(-(value + 1));
                // Clear upper bits
                if (bitSize < 64) {
                    v &= (1ull << bitSize) - 1;
                }
                writeUint64(bytes, bitOffset, bitSize, v);
            } else {
                // For positive value, the binary representation is the same as for unsigned values
                writeUint64(bytes, bitOffset, bitSize, static_cast<uint64_t>(value));
            }
        } else if constexpr (std::is_same_v<ValueType, double>) {
            writeUint64(bytes, bitOffset, bitSize, std::bit_cast<uint64_t>(value));
        } else if constexpr (std::is_same_v<ValueType, Integer>) {
            if (value < 0) {
                writeInteger<true>(bytes, bitOffset, bitSize, value);
            } else {
                writeInteger<false>(bytes, bitOffset, bitSize, value);
            }
        } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            STORM_LOG_ASSERT(bitSize % 2 == 0, "Uneven bitsize for rational number not expected.");
            auto const numDenSize = bitSize / 2;
            writeInteger<true>(bytes, bitOffset, numDenSize, storm::utility::numerator(value));
            static_assert(storm::RationalNumberDenominatorAlwaysPositive);
            writeInteger<false>(bytes, bitOffset + numDenSize, numDenSize, storm::utility::denominator(value));
        } else {
            // Note: overwriting the string does not erase the old string from the strings vector as it might still be in use elsewhere
            static_assert(std::is_same_v<ValueType, std::string_view> || std::is_same_v<ValueType, std::string>);
            uint64_t const index = storm::umb::StringsBuilder(strings, stringMapping).findOrPushBack(value);
            writeUint64(bytes, bitOffset, bitSize, index);
        }
    }

    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes>
    void write(uint64_t entity, VariableInformation const& varInfo, auto const& callback) {
        auto invokeCallback = [this, &entity, &varInfo, &callback]<typename ValueType>() -> bool {
            bool constexpr IsAllowed = (sizeof...(AllowedTypes) == 0) || std::disjunction_v<std::is_same<ValueType, AllowedTypes>...>;
            if constexpr (IsAllowed) {
                // Initialize with current value (if requested)
                ValueType value;
                bool isOptional = varInfo.description.isOptional.value_or(false);
                bool initializeAsUnsetOptional = false;
                if constexpr (InitializeWithCurrent) {
                    read<std::nullopt_t, ValueType>(entity, varInfo, [&value, &initializeAsUnsetOptional](auto..., auto&& currentValue) {
                        if constexpr (std::is_same_v<std::remove_cvref<decltype(currentValue)>, ValueType>) {
                            value = currentValue;
                        } else {
                            static_assert(std::is_same_v<std::remove_cvref<decltype(currentValue)>, std::nullopt_t>);
                            initializeAsUnsetOptional = true;
                        }
                    });
                } else {
                    initializeAsUnsetOptional = isOptional;
                    if constexpr (std::is_same_v<ValueType, uint64_t> || std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, Integer>) {
                        // Explicitly initialize integer values to the lower bound, 0, or the upper bound (in that order
                        if (varInfo.description.lower && (!std::is_same_v<ValueType, uint64_t> || varInfo.description.lower.value() >= 0)) {
                            value = storm::utility::convertNumber<ValueType>(varInfo.description.lower.value());
                        } else if (!varInfo.description.upper || varInfo.description.upper >= 0) {
                            value = storm::utility::zero<ValueType>();
                        } else {
                            value = storm::utility::convertNumber<ValueType>(varInfo.description.upper.value());
                        }
                    }
                }
                // Invoke the callback. Determine if we need to write a value and ensure that `value` holds the value to write.
                bool haveToWriteValue = false;
                if (isOptional) {
                    if constexpr (AllowOptional) {
                        // invoke callback with std::optional&
                        std::optional<ValueType> optionalValue = initializeAsUnsetOptional ? std::optional<ValueType>() : value;
                        callback(entity, varInfo.expressionVariable, optionalValue);
                        if (optionalValue.has_value()) {
                            value = std::move(optionalValue.value());
                            haveToWriteValue = true;
                        }
                        STORM_LOG_ASSERT(varInfo.bitOffset > 0, "Invalid variable information: optional variable must have a preceding presence bit.");
                        writeBit(getRawBytes(entity), varInfo.bitOffset - 1, optionalValue.has_value());
                    } else {
                        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                        "Writing to optional variable " << varInfo.description.name << " was not expected.");
                    }
                } else {
                    callback(entity, varInfo.expressionVariable, value);
                    haveToWriteValue = true;
                }
                if (haveToWriteValue) {
                    // Apply the offset for integer variables if necessary
                    if constexpr (std::is_same_v<ValueType, uint64_t> || std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, Integer>) {
                        if (auto offset = varInfo.description.offset.value_or(0); offset != 0) {
                            if constexpr (std::is_same_v<ValueType, uint64_t>) {
                                STORM_LOG_ASSERT(value >= offset, "Set negative value " << value << "-" << offset << " to unsigned variable.");
                                value -= offset;
                            } else {
                                value -= storm::utility::convertNumber<ValueType>(offset);
                            }
                        }
                    }
                    // Write the value
                    writeValue(getRawBytes(entity), varInfo.bitOffset, varInfo.description.type.bitSize(), value);
                }
            }
            return IsAllowed;
        };

        using enum storm::umb::Type;
        switch (varInfo.description.type.type) {
            case Bool:
                if (invokeCallback.template operator()<bool>()) {
                    return;
                }
                break;
            case Uint:
                if (varInfo.fits64Bit) {
                    if (varInfo.description.offset.value_or(0) >= 0) {
                        // non-negative offset, default output type is uint64_t
                        if (invokeCallback.template operator()<uint64_t>()) {
                            return;
                        }
                    }
                    // take int64_t if uint64_t is not allowed or we have a negative offset
                    if (invokeCallback.template operator()<int64_t>()) {
                        return;
                    }
                }
                // finally take Integer if the 64 bit types are not allowed or insuficient
                if (invokeCallback.template operator()<Integer>()) {
                    return;
                }
                break;
            case Int:
                // Prefer int64_t if it fits and is allowed.
                if ((varInfo.fits64Bit && invokeCallback.template operator()<int64_t>()) || invokeCallback.template operator()<Integer>()) {
                    return;
                }
                break;
            case Double:
                if (invokeCallback.template operator()<double>()) {
                    return;
                }
                break;
            case Rational:
                if (invokeCallback.template operator()<storm::RationalNumber>()) {
                    return;
                }
                break;
            case String:
                if (invokeCallback.template operator()<std::string>()) {
                    return;
                }
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Valuations for variable type '" << varInfo.description.type.toString() << "' are not supported.");
        }
    }
};
}  // namespace storm::umb