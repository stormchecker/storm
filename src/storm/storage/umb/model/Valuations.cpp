#include "storm/storage/umb/model/Valuations.h"

#include <bitset>
#include <cstring>
#include <ranges>

#include "storm/storage/BitVector.h"
#include "storm/storage/umb/model/ValueEncoding.h"
#include "storm/utility/bitoperations.h"

#include "storm/exceptions/IllegalFunctionCallException.h"

namespace storm::umb {

// ============================================================
// detail helpers (used by constructors)
// ============================================================

namespace detail {

bool fits64Bit(ValuationClassDescription::Variable const& varDesc) {
    using enum storm::umb::Type;
    if (varDesc.type.bitSize() > 64) {
        return false;
    }
    switch (varDesc.type.type) {
        case Bool:
            return true;
        case Uint: {
            // The smallest possible value is 0 + offset. As the offset is given as int64_t, it always fits into 64 bits.
            // We have to check if the largest possible value also fits.
            if (varDesc.offset.value_or(0) == 0) {
                // The offset is 0 and the bitSize is <= 64, so this always fits
                return true;
            } else if (varDesc.upper.has_value()) {
                // If the upper bound is given (as int64_t), then the largest value to represent is max(upper, upper - offset).
                // Since all numbers involved are given as int64_t, the max value will fit into 64 bits as well.
                return true;
            } else if (varDesc.type.bitSize() == 64) {
                // The largest actual value is 2^64 - 1 + offset.
                // This never fits into 64 bits for positive offset.
                // For negative offsets, the actual value type would be int64_t. Then the above number only fits if offset = -2^63
                // Since offset != 0 in this branch, that is the only valid case where the values fit into 64 bits.
                return varDesc.offset.value() == std::numeric_limits<int64_t>::min();
            } else {
                // The largest actual value is at most 2^63 - 1 + offset
                // For positive offset, the actual type is uint64_t and we can upper bound the above number by 2^63 - 1 + 2^63 - 1 = 2^64 - 2 < uint64_t max
                // For negative offset, the actual type is int64_t and we can upper bound the above number by 2^63 - 1 <= int64_t max
                return true;
            }
        }
        case Int: {
            if (varDesc.offset.value_or(0) == 0) {
                return true;
            } else if (varDesc.offset.value() < 0) {
                // negative offset. We might have trouble representing the smallest actual value
                int64_t const minValueStored =
                    varDesc.type.bitSize() == 64 ? std::numeric_limits<int64_t>::min() : -(static_cast<int64_t>(1) << (varDesc.type.bitSize() - 1));
                return varDesc.lower.has_value() || minValueStored >= std::numeric_limits<int64_t>::min() - varDesc.offset.value();
            } else {
                // positive offset. We might have trouble representing the largest actual value
                int64_t const maxValueStored =
                    varDesc.type.bitSize() == 64 ? std::numeric_limits<int64_t>::max() : (static_cast<int64_t>(1) << (varDesc.type.bitSize() - 1)) - 1;
                return varDesc.upper.has_value() || maxValueStored <= std::numeric_limits<int64_t>::max() - varDesc.offset.value();
            }
        }
        case Double:
            return true;  // double values are always stored as 64 bit IEEE 754 values
        case Rational:
            return false;
        case String:
            return true;  // string indices are always stored as uint64_t
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Valuations for variable type '" << varDesc.type.toString() << "' are not supported.");
    }
}

template<typename ManagerType>
Valuations::VariablesInformation createVariablesInformation(ManagerType& expressionManager, ValuationClassDescription const& description) {
    std::vector<typename Valuations::VariableInformation> variables;
    uint64_t currentOffset = 0;
    for (auto const& varVariant : description.variables) {
        if (std::holds_alternative<ValuationClassDescription::Variable>(varVariant)) {
            auto const& varDesc = std::get<ValuationClassDescription::Variable>(varVariant);
            storm::expressions::Variable exprVar;
            if constexpr (std::is_const_v<ManagerType>) {
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
            variables.emplace_back(typename Valuations::VariableInformation{
                .expressionVariable = exprVar, .description = varDesc, .bitOffset = currentOffset, .fits64Bit = fits64Bit(varDesc)});
            currentOffset += variables.back().description.type.bitSize();
        } else {
            auto const& padding = std::get<ValuationClassDescription::Padding>(varVariant);
            currentOffset += padding.padding;
        }
    }
    STORM_LOG_ASSERT(currentOffset == description.sizeInBits(), "Computed size does not match description size.");
    STORM_LOG_ASSERT(currentOffset % 8 == 0, "Invalid valuation description detected: size in bits must be a multiple of 8.");
    return typename Valuations::VariablesInformation{
        .variables = std::move(variables), .expressionManager = expressionManager.shared_from_this(), .sizeInBytes = currentOffset / 8};
}

}  // namespace detail

// ============================================================
// Constructors
// ============================================================

Valuations::Valuations(uint64_t const numEntities, std::vector<ValuationClassDescription> const& descriptions, std::vector<char> valuations,
                       std::vector<uint64_t> stringMapping, std::vector<char> strings, std::optional<std::vector<uint32_t>> classes,
                       std::vector<std::shared_ptr<storm::expressions::ExpressionManager const>> expressionManagers)
    : numEntities(numEntities), valuations(std::move(valuations)), stringMapping(std::move(stringMapping)), strings(std::move(strings)) {
    STORM_LOG_ASSERT(descriptions.size() == expressionManagers.size() || expressionManagers.size() <= 1,
                     "Mismatch between number of descriptions and expression managers.");
    // First set up the variable classes.
    // We either have a separate manager for each class or all classes share the same manager.
    // Furthermore, we might create a new manager for a class, if no manager was given explicitly.
    auto sharedManager = std::make_shared<storm::expressions::ExpressionManager>();
    for (uint64_t i = 0; i < descriptions.size(); ++i) {
        if (expressionManagers.empty() || (expressionManagers.size() == 1 && expressionManagers.front() == nullptr)) {
            // Shared manager for all classes, not given explicitly
            variableClasses.push_back(detail::createVariablesInformation(*sharedManager, descriptions[i]));
        } else if (expressionManagers.size() == 1) {
            // Shared manager for all classes, given explicitly
            variableClasses.push_back(detail::createVariablesInformation(*expressionManagers.front(), descriptions[i]));
        } else if (expressionManagers[i] == nullptr) {
            // Separate manager for each class, not given explicitly
            auto manager = std::make_shared<storm::expressions::ExpressionManager>();
            variableClasses.push_back(detail::createVariablesInformation(*manager, descriptions[i]));
        } else {
            // Separate manager for each class, given explicitly
            variableClasses.push_back(detail::createVariablesInformation(*expressionManagers[i], descriptions[i]));
        }
    }
    // Initialize string mappings. It should be given iff there is a string variable
    bool const hasStringVariable = std::any_of(descriptions.begin(), descriptions.end(), [](auto const& classDescr) { return classDescr.hasStringVariable(); });
    if (hasStringVariable && this->stringMapping.empty()) {
        this->stringMapping.push_back(0);
    }
    STORM_LOG_ASSERT(hasStringVariable || this->stringMapping.empty(), "Non-empty string mapping given but there is no string variable.");
    STORM_LOG_ASSERT(stringMapping.empty() || this->stringMapping.back() == strings.size(),
                     "String mapping should end with the total size of the string data.");

    // Enable quick access to the right byte span for each entity.
    if (classes.has_value() && this->variableClasses.size() > 1) {
        STORM_LOG_ASSERT(numEntities == classes->size(), "Number of entities does not match class mapping size.");
        this->entityClassMappings = {std::move(*classes), std::vector<uint64_t>({0ull})};
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
        STORM_LOG_ASSERT(this->variableClasses.front().sizeInBytes == 0 || this->valuations.size() % this->variableClasses.front().sizeInBytes == 0,
                         "Valuation data size is not a multiple of the unique valuation size.");
        STORM_LOG_ASSERT(numEntities * this->variableClasses.front().sizeInBytes == this->valuations.size(),
                         "Valuation data size (" << valuations.size() << ") does not match number of entities (" << this->numEntities
                                                 << ") times valuation size (" << this->variableClasses.front().sizeInBytes << ").");
    }
}

Valuations::Valuations(uint64_t const numEntities, ValuationClassDescription const& description, std::vector<char> valuations,
                       std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager)
    : Valuations(numEntities, {description}, std::move(valuations), {}, {}, std::nullopt, {expressionManager}) {
    STORM_LOG_ASSERT(!description.hasStringVariable(), "String mapping must be given for descriptions with string variables.");
}

Valuations::Valuations(std::vector<ValuationClassDescription> const& descriptions,
                       std::vector<std::shared_ptr<storm::expressions::ExpressionManager const>> expressionManagers)
    : Valuations(0, descriptions, {}, {}, {}, std::vector<uint32_t>{}, std::move(expressionManagers)) {}

Valuations::Valuations(ValuationClassDescription const& description, std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager)
    : Valuations(0, {description}, {}, {}, {}, std::nullopt, {expressionManager}) {}

Valuations::Valuations(std::vector<VariablesInformation> const& variableClasses) : variableClasses(variableClasses), numEntities(0) {}

// ============================================================
// Size / count queries
// ============================================================

uint64_t Valuations::size() const {
    return numEntities;
}

uint64_t Valuations::numClasses() const {
    return variableClasses.size();
}

uint64_t Valuations::numStrings() const {
    return stringMapping.size() > 0 ? stringMapping.size() - 1 : 0;
}

bool Valuations::hasStrings() const {
    return !stringMapping.empty();
}

// ============================================================
// Class and entity queries
// ============================================================

uint64_t Valuations::getClassOfEntity(uint64_t entity) const {
    STORM_LOG_ASSERT(entity < size(), "Entity index out of bounds: " << entity << " >= " << size() << ".");
    if (entityClassMappings) {
        return entityClassMappings->toClassMapping[entity];
    } else {
        STORM_LOG_ASSERT(variableClasses.size() == 1, "No class mapping given but multiple classes exist.");
        return 0;
    }
}

ValuationClassDescription Valuations::getClassDescription(uint64_t classIndex) const {
    STORM_LOG_ASSERT(classIndex < numClasses(), "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
    ValuationClassDescription res;
    uint64_t currBit = 0;
    for (auto const& varInfo : variableClasses[classIndex].variables) {
        uint64_t padding = varInfo.bitOffset - currBit;
        if (varInfo.description.isOptional.value_or(false)) {
            STORM_LOG_ASSERT(padding >= 1, "Optional variables must have at least 1 bit preceding its offset.");
            --padding;
        }
        if (padding > 0) {
            res.variables.push_back(storm::umb::ValuationClassDescription::Padding(padding));
        }
        res.variables.push_back(varInfo.description);
        currBit = varInfo.bitOffset + varInfo.description.type.bitSize();
        STORM_LOG_ASSERT(currBit == res.sizeInBits(), "Unexpected bit offset for variable " << varInfo.description.name << " in class " << classIndex
                                                                                            << ". Expected " << res.sizeInBits() << ", got "
                                                                                            << varInfo.bitOffset << ".");
    }
    if (uint64_t padding = currBit % 8; padding > 0) {
        res.variables.push_back(storm::umb::ValuationClassDescription::Padding(8 - padding));
    }
    return res;
}

storm::expressions::ExpressionManager const& Valuations::getManager() const {
    auto const& manager = variableClasses.front().expressionManager;
    STORM_LOG_THROW(
        std::all_of(variableClasses.begin(), variableClasses.end(), [&manager](auto const& varClass) { return varClass.expressionManager == manager; }),
        storm::exceptions::IllegalFunctionCallException, "Expression manager is not unique.");
    return *manager;
}

storm::expressions::ExpressionManager const& Valuations::getManager(uint64_t classIndex) const {
    STORM_LOG_ASSERT(classIndex < variableClasses.size(),
                     "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
    return *variableClasses[classIndex].expressionManager;
}

// ============================================================
// Variable lookup
// ============================================================

Valuations::VariableInformation const& Valuations::getVariableInformation(uint64_t entity, storm::expressions::Variable const& variable) const {
    auto const& vars = info(entity).variables;
    auto varInfoIt = std::find_if(vars.begin(), vars.end(), [&variable](auto const& varInfo) { return varInfo.expressionVariable == variable; });
    STORM_LOG_ASSERT(varInfoIt != vars.end(), "Can not find unknown variable " << variable.getName() << ".");
    return *varInfoIt;
}

Valuations::VariableInformation const& Valuations::getVariableInformation(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(numClasses() == 1, "Trying to get variable information but the class is not unique among entities.");
    return getVariableInformation(0, variable);
}

std::set<storm::expressions::Variable> Valuations::getAllVariables() const {
    std::set<storm::expressions::Variable> result;
    for (auto const& varClass : variableClasses) {
        for (auto const& varInfo : varClass.variables) {
            result.insert(varInfo.expressionVariable);
        }
    }
    return result;
}

bool Valuations::entityHasVariable(uint64_t entity, storm::expressions::Variable const& variable) const {
    auto const& vars = info(entity).variables;
    return std::any_of(vars.begin(), vars.end(), [&variable](auto const& varInfo) { return varInfo.expressionVariable == variable; });
}

// ============================================================
// Raw data access
// ============================================================

std::span<char const> Valuations::getRawBytes(uint64_t entity) const {
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

std::span<char> Valuations::getRawBytes(uint64_t entity) {
    if (entityClassMappings) {
        auto const start = entityClassMappings->toValuationsMapping[entity];
        auto const end = entityClassMappings->toValuationsMapping[entity + 1];
        return std::span<char>(&valuations[start], end - start);
    } else {
        auto const start = entity * variableClasses.front().sizeInBytes;
        return std::span<char>(&valuations[start], variableClasses.front().sizeInBytes);
    }
}

storm::umb::UmbModel::Valuation Valuations::getRawUmbData() const {
    storm::umb::UmbModel::Valuation result;
    if (entityClassMappings.has_value()) {
        result.valuationToClass = entityClassMappings->toClassMapping;
    }
    result.valuations = valuations;
    if (hasStrings()) {
        result.stringMapping = stringMapping;
        result.strings = strings;
    }
    return result;
}

// ============================================================
// Mutation
// ============================================================

void Valuations::resize(uint64_t newEntityCount, uint64_t const classIndex) {
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
        uint64_t const newValuationsSize =
            entityClassMappings.has_value() ? entityClassMappings->toValuationsMapping[newEntityCount] : newEntityCount * variableClasses.front().sizeInBytes;
        valuations.resize(newValuationsSize);
        if (entityClassMappings) {
            entityClassMappings->toClassMapping.resize(newEntityCount);
            entityClassMappings->toValuationsMapping.resize(newEntityCount + 1);
        }
        numEntities = newEntityCount;
    }
}

// ============================================================
// Private helpers
// ============================================================

Valuations::VariablesInformation const& Valuations::info(uint64_t entity) const {
    return variableClasses[getClassOfEntity(entity)];
}

// ============================================================
// Low-level bit I/O
// ============================================================

bool Valuations::readBit(std::span<char const> bytes, uint64_t const position) const {
    STORM_LOG_ASSERT(position < bytes.size() * 8, "Bit position exceeds valuation size.");
    return bytes[position / 8] & (1 << (position % 8));
}

void Valuations::writeBit(std::span<char> bytes, uint64_t const position, bool value) const {
    STORM_LOG_ASSERT(position < bytes.size() * 8, "Bit position exceeds valuation size.");
    char& byte = bytes[position / 8];
    char const pos = (1 << (position % 8));
    if (value) {
        byte |= pos;
    } else {
        byte &= ~pos;
    }
}

uint64_t Valuations::readUint64(std::span<char const> bytes, uint64_t const bitOffset, uint64_t const bitSize) const {
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

void Valuations::writeUint64(std::span<char> bytes, uint64_t const bitOffset, uint64_t const bitSize, uint64_t const value) const {
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

// ============================================================
// Integer encoding (arbitrary precision)
// ============================================================

template<bool Signed>
Valuations::Integer Valuations::readInteger(std::span<char const> bytes, uint64_t const bitOffset, uint64_t const bitSize) const {
    auto const num64BitChunks = (bitSize + 63) / 64;
    auto chunksView = std::ranges::iota_view(0ull, num64BitChunks) | std::ranges::views::transform([this, &bytes, &bitOffset, &bitSize](auto i) -> uint64_t {
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

template Valuations::Integer Valuations::readInteger<false>(std::span<char const>, uint64_t, uint64_t) const;
template Valuations::Integer Valuations::readInteger<true>(std::span<char const>, uint64_t, uint64_t) const;

template<bool Signed>
void Valuations::writeInteger(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, Integer const& value) const {
    auto const num64BitChunks = (bitSize + 63) / 64;
    std::vector<uint64_t> uint64Encoding;
    ValueEncoding::appendEncodedInteger<Signed>(uint64Encoding, value, num64BitChunks);
    STORM_LOG_ASSERT(uint64Encoding.size() == num64BitChunks, "Encoding does not fit into the specified bit size.");
    for (auto v : uint64Encoding) {
        if (bitSize >= 64) {
            writeUint64(bytes, bitOffset, 64, v);
            bitOffset += 64;
            bitSize -= 64;
        } else {
            uint64_t const relevantBitMask = (1ull << bitSize) - 1;
            // Check if the number is negative by looking at the sign bit
            if (Signed && ((v & (1ull << (bitSize - 1))) != 0)) {
                STORM_LOG_ASSERT(value < 0, "Value " << value << " is non-negative but the sign bit is set.");
                // Assert that all irrelevant bits are set
                STORM_LOG_ASSERT((~relevantBitMask & v) == ~relevantBitMask,
                                 "Value " << value << " does not fit into the specified bit size of " << bitSize << " bits.");
                // For negative numbers, we clear the upper (unused) bits, so that the resulting (unsigned) value fits into
                // the specified bit size. For example, -3 with bitSize=3 would be represented as 101. We get the 64 bit
                // value 1...1101 which (interpreted as unsigned value) doesn't fit into 3 bits. We need 0...0101.
                v &= relevantBitMask;  // set upper bits to zero
            } else {
                // Assert that all irrelevant bits are not set
                STORM_LOG_ASSERT((~relevantBitMask & v) == 0, "Value " << value << " does not fit into the specified bit size of " << bitSize << " bits.");
            }
            writeUint64(bytes, bitOffset, bitSize, v);
            bitSize = 0;
            break;
        }
    }
    STORM_LOG_ASSERT(bitSize == 0, "Unexpected integer encoding. Not all bits were written.");
}

template void Valuations::writeInteger<false>(std::span<char>, uint64_t, uint64_t, Integer const&) const;
template void Valuations::writeInteger<true>(std::span<char>, uint64_t, uint64_t, Integer const&) const;

// ============================================================
// Typed value encoding
// ============================================================

template<typename ValueType>
void Valuations::writeValue(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, ValueType const& value) {
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

template void Valuations::writeValue<bool>(std::span<char>, uint64_t, uint64_t, bool const&);
template void Valuations::writeValue<uint64_t>(std::span<char>, uint64_t, uint64_t, uint64_t const&);
template void Valuations::writeValue<int64_t>(std::span<char>, uint64_t, uint64_t, int64_t const&);
template void Valuations::writeValue<double>(std::span<char>, uint64_t, uint64_t, double const&);
template void Valuations::writeValue<Valuations::Integer>(std::span<char>, uint64_t, uint64_t, Valuations::Integer const&);
template void Valuations::writeValue<storm::RationalNumber>(std::span<char>, uint64_t, uint64_t, storm::RationalNumber const&);
template void Valuations::writeValue<std::string_view>(std::span<char>, uint64_t, uint64_t, std::string_view const&);
template void Valuations::writeValue<std::string>(std::span<char>, uint64_t, uint64_t, std::string const&);

// ============================================================
// Selection
// ============================================================

template<typename T>
Valuations Valuations::selectEntities(T const& selectedEntities) const {
    Valuations result(variableClasses);
    result.numEntities = [&selectedEntities]() {
        if constexpr (std::is_same_v<T, storm::storage::BitVector>) {
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

template Valuations Valuations::selectEntities<storm::storage::BitVector>(storm::storage::BitVector const&) const;
template Valuations Valuations::selectEntities<std::vector<uint64_t>>(std::vector<uint64_t> const&) const;

}  // namespace storm::umb
