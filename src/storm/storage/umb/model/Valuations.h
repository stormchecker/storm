#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/umb/model/StringEncoding.h"
#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/ValuationDescription.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm::umb {

/*!
 * Concept for a callback used in readCallback / readValue.
 * The callback is invoked as callback(entity, variable, value) where value is one of the
 * supported value types (bool, int64_t, uint64_t, double, RationalNumber, string_view, string,
 * or std::nullopt_t for unset optional variables).
 */
template<typename F>
concept ValuationReadCallback =
    std::invocable<F, uint64_t, storm::expressions::Variable const&, bool> || std::invocable<F, uint64_t, storm::expressions::Variable const&, int64_t> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, uint64_t> || std::invocable<F, uint64_t, storm::expressions::Variable const&, double> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, storm::RationalNumber> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, storm::NumberTraits<storm::RationalNumber>::IntegerType> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, std::string_view> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, std::string> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, std::nullopt_t>;

/*!
 * Concept for a callback used in writeCallback / writeValue.
 * The callback is invoked as callback(entity, variable, value) where value is passed by
 * (non-const) lvalue reference so the callback can assign the new value.
 * For optional variables (AllowOptional=true), value is wrapped in std::optional<ValueType>&.
 */
template<typename F>
concept ValuationWriteCallback =
    std::invocable<F, uint64_t, storm::expressions::Variable const&, bool&> || std::invocable<F, uint64_t, storm::expressions::Variable const&, int64_t&> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, uint64_t&> || std::invocable<F, uint64_t, storm::expressions::Variable const&, double&> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, storm::RationalNumber&> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, storm::NumberTraits<storm::RationalNumber>::IntegerType&> ||
    std::invocable<F, uint64_t, storm::expressions::Variable const&, std::string&>;

class Valuations {
   public:
    using Integer = storm::NumberTraits<storm::RationalNumber>::IntegerType;

    /*!
     * Compiled information about a single variable within a valuation class.
     * Derived from the ValuationClassDescription at construction time and cached here to avoid
     * recomputing bit offsets and type properties on every read/write.
     */
    struct VariableInformation {
        storm::expressions::Variable const expressionVariable;
        ValuationClassDescription::Variable const description;
        uint64_t const bitOffset;  // The first bit holding the variable's data within the valuation.
        // If the variable is optional, the optional bit is located at bitOffset - 1
        bool const fits64Bit;
    };

    /*!
     * Compiled information about all variables belonging to one valuation class.
     * Holds the per-variable information list, the owning ExpressionManager, and the total
     * size in bytes of one entity's packed valuation data for this class.
     */
    struct VariablesInformation {
        std::vector<VariableInformation> const variables;
        std::shared_ptr<storm::expressions::ExpressionManager const> const expressionManager;
        uint64_t const sizeInBytes;
    };

    // --- Special members ---
    Valuations(Valuations const&) = default;
    Valuations(Valuations&&) = default;
    Valuations& operator=(Valuations const&) = default;
    Valuations& operator=(Valuations&&) = default;

    // --- Constructors ---

    /*!
     * Full constructor. Creates a Valuations object from pre-existing packed data.
     * @param numEntities    Number of entities whose valuations are stored.
     * @param descriptions   One ValuationClassDescription per class.
     * @param valuations     Packed valuation bytes for all entities.
     * @param stringMapping  CSR offsets into @p strings; must end with strings.size().
     *                       Pass empty when there are no string variables.
     * @param strings        Concatenated string data referenced by @p stringMapping.
     * @param classes        Per-entity class index vector. If omitted (or all zeros), all entities
     *                       are assumed to belong to class 0. Required when @p descriptions has
     *                       more than one entry and entities differ in class.
     * @param expressionManagers  Expression managers to use for variable lookup. If empty or a
     *                       single nullptr, a fresh shared manager is created for all classes.
     *                       If a single non-null pointer is given, it is shared across all classes.
     *                       If one pointer per class is given, each class gets its own manager.
     */
    Valuations(uint64_t numEntities, std::vector<ValuationClassDescription> const& descriptions, std::vector<char> valuations,
               std::vector<uint64_t> stringMapping, std::vector<char> strings, std::optional<std::vector<uint32_t>> classes = {},
               std::vector<std::shared_ptr<storm::expressions::ExpressionManager const>> expressionManagers = {});

    /*!
     * Convenience constructor for a single-class Valuations with pre-existing packed data.
     * Equivalent to the full constructor with a single description, no class mapping, and no
     * string data. Asserts that @p description contains no string variables.
     * @param numEntities       Number of entities.
     * @param description       The single valuation class description.
     * @param valuations        Packed valuation bytes.
     * @param expressionManager Expression manager to use; a fresh one is created if null.
     */
    Valuations(uint64_t numEntities, ValuationClassDescription const& description, std::vector<char> valuations,
               std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager = {});

    /*!
     * Constructs an empty (zero-entity) Valuations ready to receive multiple classes via
     * emplaceBack / resize.
     * @param descriptions      One ValuationClassDescription per class.
     * @param expressionManagers  See full constructor for semantics.
     */
    Valuations(std::vector<ValuationClassDescription> const& descriptions,
               std::vector<std::shared_ptr<storm::expressions::ExpressionManager const>> expressionManagers = {});

    /*!
     * Constructs an empty (zero-entity) single-class Valuations ready to receive entities via
     * emplaceBack / resize.
     * @param description       The single valuation class description.
     * @param expressionManager Expression manager to use; a fresh one is created if null.
     */
    Valuations(ValuationClassDescription const& description, std::shared_ptr<storm::expressions::ExpressionManager const> expressionManager = {});

    // --- Size/count queries ---

    /*!
     * @return The number of entities (e.g. states) that this Valuations assigns values for.
     */
    uint64_t size() const;

    /*!
     * @return The number of distinct valuation classes (i.e. the number of ValuationClassDescriptions
     *         this object was constructed with).
     */
    uint64_t numClasses() const;

    /*!
     * @return The number of distinct string values stored across all string variables.
     */
    uint64_t numStrings() const;

    /*!
     * @return True iff this Valuations contains at least one string variable (and therefore
     *         maintains a non-empty string table).
     */
    bool hasStrings() const;

    // --- Class and entity queries ---

    /*!
     * Returns the class index of the given entity.
     * When all entities share a single class this always returns 0.
     * @param entity  Entity index; must be less than size().
     */
    uint64_t getClassOfEntity(uint64_t entity) const;

    /*!
     * Reconstructs the ValuationClassDescription for the given class from the stored compiled
     * variable information, including any padding entries.
     * @param classIndex  Class index; must be less than numClasses(). Defaults to 0.
     */
    ValuationClassDescription getClassDescription(uint64_t classIndex = 0) const;

    /*!
     * Returns the expression manager shared by all classes.
     * @throws IllegalFunctionCallException if different classes use different managers.
     */
    storm::expressions::ExpressionManager const& getManager() const;

    /*!
     * Returns the expression manager for the given class.
     * @param classIndex  Class index; must be less than numClasses().
     */
    storm::expressions::ExpressionManager const& getManager(uint64_t classIndex) const;

    // --- Variable lookup ---

    /*!
     * Returns all expression variables that this valuation assigns values to for at least one class.
     */
    std::set<storm::expressions::Variable> getAllVariables() const;

    /*!
     * Returns true iff the variable is relevant for the given entity's class, i.e. it is listed in the class description
     */
    bool entityHasVariable(uint64_t entity, storm::expressions::Variable const& variable) const;

    /*!
     * Looks up compiled variable information for the given entity and variable.
     * The lookup is performed in the class that @p entity belongs to.
     * @param entity    Entity index; must be less than size().
     * @param variable  An expression variable registered in the corresponding ExpressionManager.
     */
    VariableInformation const& getVariableInformation(uint64_t entity, storm::expressions::Variable const& variable) const;

    /*!
     * Looks up compiled variable information for the given variable.
     * Only valid when numClasses() == 1 (asserts otherwise); uses entity 0 for the lookup.
     * @param variable  An expression variable registered in the ExpressionManager.
     */
    VariableInformation const& getVariableInformation(storm::expressions::Variable const& variable) const;

    // --- Raw data access ---

    /*!
     * Returns a read-only view of the packed valuation bytes for the given entity.
     * @param entity  Entity index; must be less than size().
     */
    std::span<char const> getRawBytes(uint64_t entity) const;

    /*!
     * Returns a mutable view of the packed valuation bytes for the given entity.
     * @param entity  Entity index; must be less than size().
     */
    std::span<char> getRawBytes(uint64_t entity);

    /*!
     * Exports a snapshot of the raw UMB model valuation data (packed bytes, optional class
     * mapping, and string table) in the format expected by UmbModel::Valuation.
     */
    typename storm::umb::UmbModel::Valuation getRawUmbData() const;

    // --- Mutation ---

    /*!
     * Resizes the entity count to @p newEntityCount.
     * When growing, new entities are appended to @p classIndex and initialized with default
     * values (lower bound for integers, false for booleans, etc.). When shrinking, the trailing
     * entities are removed.  No-op if newEntityCount == size().
     * @param newEntityCount  Desired number of entities after the call.
     * @param classIndex      Class to use for newly appended entities. Defaults to 0.
     */
    void resize(uint64_t newEntityCount, uint64_t classIndex = 0);

    /*!
     * Appends a new entity of the given class and populates its variables via @p callback.
     * The callback is invoked once per variable with the entity index, the variable, and a
     * mutable reference to the (default-initialized) value to be written.
     * @tparam AllowOptional  If true, the callback receives std::optional<ValueType>& for
     *                        optional variables, allowing them to be left unset (std::nullopt).
     * @tparam AllowedTypes   If non-empty, only variables whose natural type is in this list
     *                        will be passed to the callback; others are silently skipped.
     * @tparam Callback       A type satisfying ValuationWriteCallback.
     * @param classIndex      The class index for the new entity.
     * @param callback        Callable invoked as callback(entity, variable, value).
     */
    template<bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void emplaceBack(uint64_t classIndex, Callback const& callback) {
        STORM_LOG_ASSERT(classIndex < variableClasses.size(),
                         "Class index " << classIndex << " out of bounds. Only " << variableClasses.size() << "classes known.");
        valuations.resize(valuations.size() + variableClasses[classIndex].sizeInBytes, 0);
        if (entityClassMappings) {
            entityClassMappings->toClassMapping.push_back(classIndex);
            entityClassMappings->toValuationsMapping.push_back(valuations.size());
        }
        ++numEntities;
        writeCallback<false, AllowOptional, AllowedTypes...>(size() - 1, callback);
    }

    /*!
     * Convenience overload of emplaceBack for single-class Valuations (asserts numClasses() == 1).
     * @tparam AllowOptional  See emplaceBack(classIndex, callback).
     * @tparam AllowedTypes   See emplaceBack(classIndex, callback).
     * @tparam Callback       A type satisfying ValuationWriteCallback.
     * @param callback        Callable invoked as callback(entity, variable, value).
     */
    template<bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void emplaceBack(Callback const& callback) {
        STORM_LOG_ASSERT(variableClasses.size() == 1, "Trying to add a valuation but the class is not unique.");
        emplaceBack<AllowOptional, AllowedTypes...>(0, callback);
    }

    /*!
     * Constructs a new Valuations containing only the selected entities, in the order they
     * appear in @p selectedEntities.  The string table is shared (copied) verbatim.
     * @tparam T  Either storm::storage::BitVector or any range of uint64_t entity indices.
     * @param selectedEntities  The set or sequence of entity indices to include.
     * @return A new Valuations with size() equal to the number of selected entities.
     */
    template<typename T>
    Valuations selectEntities(T const& selectedEntities) const;

    // --- Read operations ---

    /*!
     * Reads all variables of the given entity and invokes @p callback for each one.
     * @tparam AllowedTypes  If non-empty, the callback is only invoked for variables whose
     *                       natural value type is among these types.
     * @tparam Callback      A type satisfying ValuationReadCallback.
     * @param entity         Entity index; must be less than size().
     * @param callback       Callable invoked as callback(entity, variable, value).
     */
    template<typename... AllowedTypes, ValuationReadCallback Callback>
    void readCallback(uint64_t entity, Callback const& callback) const {
        for (auto const& varInfo : info(entity).variables) {
            read<AllowedTypes...>(entity, varInfo, callback);
        }
    }

    /*!
     * Reads a single variable of the given entity and invokes @p callback with its value.
     * @tparam AllowedTypes  See readCallback(entity, callback).
     * @tparam Callback      A type satisfying ValuationReadCallback.
     * @param entity         Entity index; must be less than size().
     * @param variable       The variable to read.
     * @param callback       Callable invoked as callback(entity, variable, value).
     */
    template<typename... AllowedTypes, ValuationReadCallback Callback>
    void readCallback(uint64_t entity, storm::expressions::Variable const& variable, Callback const& callback) const {
        read<AllowedTypes...>(entity, getVariableInformation(entity, variable), callback);
    }

    /*!
     * Reads the given variable for every entity and invokes @p callback for each one.
     * When numClasses() == 1, the variable information is looked up only once for efficiency.
     * @tparam AllowedTypes  See readCallback(entity, callback).
     * @tparam Callback      A type satisfying ValuationReadCallback.
     * @param variable       The variable to read across all entities.
     * @param callback       Callable invoked as callback(entity, variable, value).
     */
    template<typename... AllowedTypes, ValuationReadCallback Callback>
    void readCallback(storm::expressions::Variable const& variable, Callback const& callback) const {
        if (numClasses() == 1) {
            // We have only one class, so we can look up the variable info once and use it for all entities
            auto const& varInfo = getVariableInformation(variable);
            for (uint64_t entity = 0; entity < size(); ++entity) {
                read<AllowedTypes...>(entity, varInfo, callback);
            }
        } else {
            for (uint64_t entity = 0; entity < size(); ++entity) {
                readCallback<AllowedTypes...>(entity, variable, callback);
            }
        }
    }

    /*!
     * Reads all variables of every entity and invokes @p callback for each (entity, variable)
     * pair.
     * @tparam AllowedTypes  See readCallback(entity, callback).
     * @tparam Callback      A type satisfying ValuationReadCallback.
     * @param callback       Callable invoked as callback(entity, variable, value).
     */
    template<typename... AllowedTypes, ValuationReadCallback Callback>
    void readCallback(Callback const& callback) const {
        for (uint64_t entity = 0; entity < size(); ++entity) {
            readCallback<AllowedTypes...>(entity, callback);
        }
    }

    /*!
     * Reads a single variable of the given entity and returns its value directly.
     * The callback type must accept exactly the concrete type of the variable; an incorrect
     * ValueType will result in a default-initialized return value.
     * @tparam ValueType  The expected C++ type of the variable (e.g. bool, int64_t, double).
     * @param entity      Entity index; must be less than size().
     * @param variable    The variable to read.
     * @return The current value of @p variable for @p entity.
     */
    template<typename ValueType>
    ValueType readValue(uint64_t entity, storm::expressions::Variable const& variable) const {
        ValueType result;
        readCallback<ValueType>(entity, variable, [&](auto, auto, ValueType value) { result = std::move(value); });
        return result;
    }

    // --- Write operations ---

    /*!
     * Writes all variables of the given entity by invoking @p callback for each one.
     * The callback receives a mutable reference to the current (or default) value and may
     * overwrite it with the desired new value.
     * @tparam InitializeWithCurrent  If true, the reference passed to the callback is
     *                                pre-initialized with the variable's current stored value
     *                                instead of the type's default.
     * @tparam AllowOptional  If true, optional variables are exposed as std::optional<ValueType>&
     *                        so the callback can leave them unset.
     * @tparam AllowedTypes   If non-empty, only variables with a matching natural type are passed
     *                        to the callback; others are silently skipped.
     * @tparam Callback       A type satisfying ValuationWriteCallback.
     * @param entity          Entity index; must be less than size().
     * @param callback        Callable invoked as callback(entity, variable, value).
     */
    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void writeCallback(uint64_t entity, Callback const& callback) {
        for (auto const& varInfo : info(entity).variables) {
            write<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, varInfo, callback);
        }
    }

    /*!
     * Writes a single variable of the given entity by invoking @p callback.
     * @tparam InitializeWithCurrent  See writeCallback(entity, callback).
     * @tparam AllowOptional          See writeCallback(entity, callback).
     * @tparam AllowedTypes           See writeCallback(entity, callback).
     * @tparam Callback               A type satisfying ValuationWriteCallback.
     * @param entity                  Entity index; must be less than size().
     * @param variable                The variable to write.
     * @param callback                Callable invoked as callback(entity, variable, value).
     */
    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void writeCallback(uint64_t entity, storm::expressions::Variable const& variable, Callback const& callback) {
        write<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, getVariableInformation(entity, variable), callback);
    }

    /*!
     * Writes all variables of every entity by invoking @p callback for each (entity, variable)
     * pair.
     * @tparam InitializeWithCurrent  See writeCallback(entity, callback).
     * @tparam AllowOptional          See writeCallback(entity, callback).
     * @tparam AllowedTypes           See writeCallback(entity, callback).
     * @tparam Callback               A type satisfying ValuationWriteCallback.
     * @param callback                Callable invoked as callback(entity, variable, value).
     */
    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void writeCallback(Callback const& callback) {
        for (uint64_t entity = 0; entity < size(); ++entity) {
            writeCallback<InitializeWithCurrent, AllowOptional, AllowedTypes...>(entity, callback);
        }
    }

    /*!
     * Directly writes @p value to the given variable of @p entity.
     * Passing std::nullopt marks an optional variable as unset.
     * Passing std::string_view is converted to std::string internally.
     * @tparam ValueType  The C++ type of the value to write.
     * @param entity      Entity index; must be less than size().
     * @param variable    The variable to write.
     * @param value       The value to store.
     */
    template<typename ValueType>
    void writeValue(uint64_t entity, storm::expressions::Variable const& variable, ValueType const& value) {
        if constexpr (std::is_same_v<ValueType, std::nullopt_t>) {
            writeCallback<false, true>(entity, variable, [&value](auto, auto, auto&) { /* intentionally empty */ });
        } else if constexpr (std::is_same_v<ValueType, std::string_view>) {
            writeCallback<false, false, std::string>(entity, variable, [&value](auto, auto, std::string& val) { val = value; });
        } else {
            writeCallback<false, false, ValueType>(entity, variable, [&value](auto, auto, ValueType& val) { val = value; });
        }
    }

   private:
    // --- Data ---
    uint64_t numEntities;
    std::vector<VariablesInformation> variableClasses;

    struct ClassData {
        std::vector<uint32_t> toClassMapping;       // mapping from entity index to class index (size equals number of entities)
        std::vector<uint64_t> toValuationsMapping;  // CSR mapping from entity index to valuations
    };
    std::optional<ClassData> entityClassMappings;  // present iff there are multiple classes

    std::vector<char> valuations;
    std::vector<uint64_t> stringMapping;
    std::vector<char> strings;

    // --- Private constructor ---
    explicit Valuations(std::vector<VariablesInformation> const& variableClasses);

    // --- Internal helpers ---
    VariablesInformation const& info(uint64_t entity) const;

    // --- Low-level bit I/O ---
    bool readBit(std::span<char const> bytes, uint64_t position) const;
    void writeBit(std::span<char> bytes, uint64_t position, bool value) const;
    uint64_t readUint64(std::span<char const> bytes, uint64_t bitOffset, uint64_t bitSize) const;
    void writeUint64(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, uint64_t value) const;

    // --- Integer encoding (arbitrary precision) ---
    template<bool Signed>
    Integer readInteger(std::span<char const> bytes, uint64_t bitOffset, uint64_t bitSize) const;

    template<bool Signed>
    void writeInteger(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, Integer const& value) const;

    // --- Typed value encoding ---
    template<typename ValueType>
    void writeValue(std::span<char> bytes, uint64_t bitOffset, uint64_t bitSize, ValueType const& value);

    // --- Core per-variable read/write ---
    /*!
     * Reads the given variable for the given entity and calls the callback with the read value.
     * @tparam AllowedTypes either empty (allowing all types) or a list of types that are handled in the callback.
     * @param entity the entity (state/choice/branch/observation index)
     * @param varInfo The info for the given variable
     * @param callback The callback.
     */
    template<typename... AllowedTypes, ValuationReadCallback Callback>
    void read(uint64_t entity, VariableInformation const& varInfo, Callback const& callback) const {
        auto invokeCallback = [&entity, &varInfo, &callback](auto&& value) -> bool {
            using ValueType = std::remove_cvref_t<decltype(value)>;
            bool constexpr IsAllowed = (sizeof...(AllowedTypes) == 0) || std::disjunction_v<std::is_same<ValueType, AllowedTypes>...>;
            if constexpr (IsAllowed) {
                callback(entity, varInfo.expressionVariable, std::move(value));
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

    template<bool InitializeWithCurrent = false, bool AllowOptional = false, typename... AllowedTypes, ValuationWriteCallback Callback>
    void write(uint64_t entity, VariableInformation const& varInfo, Callback const& callback) {
        auto invokeCallback = [this, &entity, &varInfo, &callback]<typename ValueType>() -> bool {
            bool constexpr IsAllowed = (sizeof...(AllowedTypes) == 0) || std::disjunction_v<std::is_same<ValueType, AllowedTypes>...>;
            if constexpr (IsAllowed) {
                ValueType value;
                bool isOptional = varInfo.description.isOptional.value_or(false);
                bool initializeAsUnsetOptional = false;
                if constexpr (InitializeWithCurrent) {
                    // Initialize with current value (if requested)
                    read<std::nullopt_t, ValueType>(entity, varInfo, [&value, &initializeAsUnsetOptional](auto..., auto&& currentValue) {
                        using CurrentVT = std::remove_cvref_t<decltype(currentValue)>;
                        if constexpr (std::is_same_v<CurrentVT, ValueType>) {
                            value = currentValue;
                        } else {
                            static_assert(std::is_same_v<CurrentVT, std::nullopt_t>);
                            initializeAsUnsetOptional = true;
                        }
                    });
                } else {
                    initializeAsUnsetOptional = isOptional;
                    if constexpr (std::is_same_v<ValueType, uint64_t> || std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, Integer>) {
                        // Explicitly initialize integer values to the lower bound, 0, or the upper bound (in that order)
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
                // finally take Integer if the 64 bit types are not allowed or insufficient
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
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                        "Variable " << varInfo.description.name << " of type " << varInfo.description.type.toString() << " is not handled.");
    }
};
}  // namespace storm::umb
