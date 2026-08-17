/*
 * Copyright 2011-2015 Formal Methods and Tools, University of Twente
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYLVAN_OBJ_H
#define SYLVAN_OBJ_H

#include <string>
#include <vector>

#include <lace.h>
#include <sylvan.h>

/////////////////////
// ADDED BY STORM
/////////////////////

// For the Mtbdd extensions
#include "storm/adapters/RationalFunctionForward.h"

/////////////////////

namespace sylvan {

class BddSet;
class BddMap;

/////////////////////
// ADDED BY STORM
/////////////////////

// forward declare Mtbdd
// used to add convert functions directly to Bdd
class Mtbdd;

/////////////////////

class Bdd {
    friend class Sylvan;
    friend class BddSet;
    friend class BddMap;
    friend class Mtbdd;

public:
    Bdd() { bdd = sylvan_false; sylvan_protect(&bdd); }
    Bdd(const BDD from) : bdd(from) { sylvan_protect(&bdd); }
    Bdd(const Bdd &from) : bdd(from.bdd) { sylvan_protect(&bdd); }
    Bdd(const uint32_t var) { bdd = sylvan_ithvar(var); sylvan_protect(&bdd); }
    ~Bdd() { sylvan_unprotect(&bdd); }

    /**
     * @brief Creates a Bdd representing just the variable index in its positive form
     * The variable index must be a 0<=index<=2^23 (we use 24 bits internally)
     */
    static Bdd bddVar(uint32_t index);

    /**
     * @brief Returns the Bdd representing "True"
     */
    static Bdd bddOne();

    /**
     * @brief Returns the Bdd representing "False"
     */
    static Bdd bddZero();

    /**
     * @brief Returns the Bdd representing a cube of variables, according to the given values.
     * @param variables the variables that will be in the cube in their positive or negative form
     * @param values a character array describing how the variables will appear in the result
     * The length of string must be equal to the number of variables in the cube.
     * For every ith char in string, if it is 0, the corresponding variable will appear in its negative form,
     * if it is 1, it will appear in its positive form, and if it is 2, it will appear as "any", thus it will
     * be skipped.
     */
    static Bdd bddCube(const BddSet &variables, unsigned char *values);

    /**
     * @brief Returns the Bdd representing a cube of variables, according to the given values.
     * @param variables the variables that will be in the cube in their positive or negative form
     * @param string a character array describing how the variables will appear in the result
     * The length of string must be equal to the number of variables in the cube.
     * For every ith char in string, if it is 0, the corresponding variable will appear in its negative form,
     * if it is 1, it will appear in its positive form, and if it is 2, it will appear as "any", thus it will
     * be skipped.
     */
    static Bdd bddCube(const BddSet &variables, std::vector<uint8_t> values);

    bool operator==(const Bdd& other) const;
    bool operator!=(const Bdd& other) const;
    Bdd& operator=(const Bdd& right);
    bool operator<=(const Bdd& other) const;
    bool operator>=(const Bdd& other) const;
    bool operator<(const Bdd& other) const;
    bool operator>(const Bdd& other) const;
    Bdd operator!() const;
    Bdd operator~() const;
    Bdd operator*(const Bdd& other) const;
    Bdd& operator*=(const Bdd& other);
    Bdd operator&(const Bdd& other) const;
    Bdd& operator&=(const Bdd& other);
    Bdd operator+(const Bdd& other) const;
    Bdd& operator+=(const Bdd& other);
    Bdd operator|(const Bdd& other) const;
    Bdd& operator|=(const Bdd& other);
    Bdd operator^(const Bdd& other) const;
    Bdd& operator^=(const Bdd& other);
    Bdd operator-(const Bdd& other) const;
    Bdd& operator-=(const Bdd& other);

    /**
     * @brief Returns non-zero if this Bdd is bddOne() or bddZero()
     */
    bool isConstant() const;

    /**
     * @brief Returns non-zero if this Bdd is bddOne() or bddZero()
     */
    bool isTerminal() const;

    /**
     * @brief Returns non-zero if this Bdd is bddOne()
     */
    bool isOne() const;

    /**
     * @brief Returns non-zero if this Bdd is bddZero()
     */
    bool isZero() const;

    /**
     * @brief Returns the top variable index of this Bdd (the variable in the root node)
     */
    uint32_t TopVar() const;

    /**
     * @brief Follows the high edge ("then") of the root node of this Bdd
     */
    Bdd Then() const;

    /**
     * @brief Follows the low edge ("else") of the root node of this Bdd
     */
    Bdd Else() const;

    /**
     * @brief Computes \exists cube: f \and g
     */
    Bdd AndAbstract(const Bdd& g, const BddSet& cube) const;

    /**
     * @brief Computes \exists cube: f
     */
    Bdd ExistAbstract(const BddSet& cube) const;

    /**
     * @brief Computes \forall cube: f
     */
    Bdd UnivAbstract(const BddSet& cube) const;

    /**
     * @brief Computes if f then g else h
     */
    Bdd Ite(const Bdd& g, const Bdd& h) const;

    /**
     * @brief Computes f \and g
     */
    Bdd And(const Bdd& g) const;

    /**
     * @brief Computes f \or g
     */
    Bdd Or(const Bdd& g) const;

    /**
     * @brief Computes \not (f \and g)
     */
    Bdd Nand(const Bdd& g) const;

    /**
     * @brief Computes \not (f \or g)
     */
    Bdd Nor(const Bdd& g) const;

    /**
     * @brief Computes f \xor g
     */
    Bdd Xor(const Bdd& g) const;

    /**
     * @brief Computes \not (f \xor g), i.e. f \equiv g
     */
    Bdd Xnor(const Bdd& g) const;

    /**
     * @brief Returns whether all elements in f are also in g
     */
    bool Leq(const Bdd& g) const;

    /**
     * @brief Computes the reverse application of a transition relation to this set.
     * @param relation the transition relation to apply
     * @param cube the variables that are in the transition relation
     * This function assumes that s,t are interleaved with s even and t odd (s+1).
     * Other variables in the relation are ignored (by existential quantification)
     * Set cube to "false" (illegal cube) to assume all encountered variables are in s,t
     *
     * Use this function to concatenate two relations   --> -->
     * or to take the 'previous' of a set               -->  S
     */
    Bdd RelPrev(const Bdd& relation, const BddSet& cube) const;

    /**
     * @brief Computes the application of a transition relation to this set.
     * @param relation the transition relation to apply
     * @param cube the variables that are in the transition relation
     * This function assumes that s,t are interleaved with s even and t odd (s+1).
     * Other variables in the relation are ignored (by existential quantification)
     * Set cube to "false" (illegal cube) to assume all encountered variables are in s,t
     *
     * Use this function to take the 'next' of a set     S  -->
     */
    Bdd RelNext(const Bdd& relation, const BddSet& cube) const;

    /**
     * @brief Computes the transitive closure by traversing the BDD recursively.
     * See Y. Matsunaga, P. C. McGeer, R. K. Brayton
     *     On Computing the Transitive Closre of a State Transition Relation
     *     30th ACM Design Automation Conference, 1993.
     */
    Bdd Closure() const;

    /**
     * @brief Computes the constrain f @ c
     */
    Bdd Constrain(const Bdd &c) const;

    /**
     * @brief Computes the BDD restrict according to Coudert and Madre's algorithm (ICCAD90).
     */
    Bdd Restrict(const Bdd &c) const;

    /**
     * @brief Functional composition. Whenever a variable v in the map m is found in the BDD,
     *        it is substituted by the associated function.
     * You can also use this function to implement variable reordering.
     */
    Bdd Compose(const BddMap &m) const;

    /**
     * @brief Substitute all variables in the array from by the corresponding variables in to.
     */
    Bdd Permute(const std::vector<uint32_t>& from, const std::vector<uint32_t>& to) const;

    /**
     * @brief Computes the support of a Bdd.
     */
    Bdd Support() const;

    /**
     * @brief Gets the BDD of this Bdd (for C functions)
     */
    BDD GetBDD() const;

    /**
     * @brief Writes .dot file of this Bdd. Not thread-safe!
     */
    void PrintDot(FILE *out) const;

    /**
     * @brief Gets a SHA2 hash that describes the structure of this Bdd.
     * @param string a character array of at least 65 characters (includes zero-termination)
     * This hash is 64 characters long and is independent of the memory locations of BDD nodes.
     */
    void GetShaHash(char *string) const;

    std::string GetShaHash() const;

    /**
     * @brief Computes the number of satisfying variable assignments, using variables in cube.
     */
    double SatCount(const BddSet &cube) const;

    /**
     * @brief Compute the number of satisfying variable assignments, using the given number of variables.
     */
    double SatCount(const size_t nvars) const;

    /**
     * @brief Gets one satisfying assignment according to the variables.
     * @param variables The set of variables to be assigned, must include the support of the Bdd.
     */
    void PickOneCube(const BddSet &variables, uint8_t *string) const;

    /**
     * @brief Gets one satisfying assignment according to the variables.
     * @param variables The set of variables to be assigned, must include the support of the Bdd.
     * Returns an empty vector when either this Bdd equals bddZero() or the cube is empty.
     */
    std::vector<bool> PickOneCube(const BddSet &variables) const;

    /**
     * @brief Gets a cube that satisfies this Bdd.
     */
    Bdd PickOneCube() const;

    /**
     * @brief Faster version of: *this + Sylvan::bddCube(variables, values);
     */
    Bdd UnionCube(const BddSet &variables, uint8_t *values) const;

    /**
     * @brief Faster version of: *this + Sylvan::bddCube(variables, values);
     */
    Bdd UnionCube(const BddSet &variables, std::vector<uint8_t> values) const;

    /**
     * @brief Generate a cube representing a set of variables
     */
    static Bdd VectorCube(const std::vector<Bdd> variables);

    /**
     * @brief Generate a cube representing a set of variables
     * @param variables An sorted set of variable indices
     */
    static Bdd VariablesCube(const std::vector<uint32_t> variables);

    /**
     * @brief Gets the number of nodes in this Bdd. Not thread-safe!
     */
    size_t NodeCount() const;

    /////////////////////
    // ADDED BY STORM
    /////////////////////

    // Methods to convert a BDD to the canonical 0/1 MTBDD for different types.
    Mtbdd toDoubleMtbdd() const;
    Mtbdd toInt64Mtbdd() const;
    Mtbdd toStormRationalNumberMtbdd() const;

    void PrintText(FILE *out) const;
    Mtbdd toStormRationalFunctionMtbdd() const;

    // Other functions to add to sylvan's Bdd class.
    Mtbdd Ite(Mtbdd const& thenDd, Mtbdd const& elseDd) const;
    Bdd ExistAbstractRepresentative(const BddSet& cube) const;

    Bdd Without(Bdd const& other) const;
    Bdd Minsol() const;

    /////////////////////

private:
    BDD bdd;
};

class BddSet
{
    friend class Bdd;
    friend class Mtbdd;
    Bdd set;

public:
    /**
     * @brief Create a new empty set.
     */
    BddSet() : set(Bdd::bddOne()) {}

    /**
     * @brief Wrap the BDD cube <other> in a set.
     */
    BddSet(const Bdd &other) : set(other) {}

    /**
     * @brief Create a copy of the set <other>.
     */
    BddSet(const BddSet &other) : set(other.set) {}

    /**
     * @brief Add the variable <variable> to this set.
     */
    void add(uint32_t variable) {
        set *= Bdd::bddVar(variable);
    }

    /**
     * @brief Add all variables in the set <other> to this set.
     */
    void add(BddSet &other) {
        set *= other.set;
    }

    /**
     * @brief Remove the variable <variable> from this set.
     */
    void remove(uint32_t variable) {
        set = set.ExistAbstract(Bdd::bddVar(variable));
    }

    /**
     * @brief Remove all variables in the set <other> from this set.
     */
    void remove(BddSet &other) {
        set = set.ExistAbstract(other.set);
    }

    /**
     * @brief Retrieve the head of the set. (The first variable.)
     */
    uint32_t TopVar() const {
        return set.TopVar();
    }

    /**
     * @brief Retrieve the tail of the set. (The set containing all but the first variables.)
     */
    BddSet Next() const {
        Bdd then = set.Then();
        return BddSet(then);
    }

    /**
     * @brief Return true if this set is empty, or false otherwise.
     */
    bool isEmpty() const {
        return set.isOne();
    }

    /**
     * @brief Return true if this set contains the variable <variable>, or false otherwise.
     */
    bool contains(uint32_t variable) const {
        if (isEmpty()) return false;
        else if (TopVar() == variable) return true;
        else return Next().contains(variable);
    }

    /**
     * @brief Return the number of variables in this set.
     */
    size_t size() const {
        if (isEmpty()) return 0;
        else return 1 + Next().size();
    }

    /**
     * @brief Create a set containing the <length> variables in <arr>.
     * It is advised to have the variables in <arr> in ascending order.
     */
    static BddSet fromArray(BDDVAR *arr, size_t length) {
        BddSet set;
        for (size_t i = 0; i < length; i++) {
            set.add(arr[length-i-1]);
        }
        return set;
    }

    /**
     * @brief Create a set containing the variables in <variables>.
     * It is advised to have the variables in <arr> in ascending order.
     */
    static BddSet fromVector(const std::vector<Bdd> variables) {
        BddSet set;
        for (int i=variables.size()-1; i>=0; i--) {
            set.set *= variables[i];
        }
        return set;
    }

    /**
     * @brief Create a set containing the variables in <variables>.
     * It is advised to have the variables in <arr> in ascending order.
     */
    static BddSet fromVector(const std::vector<uint32_t> variables) {
        BddSet set;
        for (int i=variables.size()-1; i>=0; i--) {
            set.add(variables[i]);
        }
        return set;
    }

    /**
     * @brief Write all variables in this set to <arr>.
     * @param arr An array of at least size this.size().
     */
    void toArray(BDDVAR *arr) const {
        if (!isEmpty()) {
            *arr = TopVar();
            Next().toArray(arr+1);
        }
    }

    /**
     * @brief Return the vector of all variables in this set.
     */
    std::vector<uint32_t> toVector() const {
        std::vector<uint32_t> result;
        Bdd x = set;
        while (!x.isOne()) {
            result.push_back(x.TopVar());
            x = x.Then();
        }
        return result;
    }
};

class BddMap
{
    friend class Bdd;
    BDD bdd;
    BddMap(const BDD from) : bdd(from) { sylvan_protect(&bdd); }
    BddMap(const Bdd &from) : bdd(from.bdd) { sylvan_protect(&bdd); }
public:
    BddMap(const BddMap& from) : bdd(from.bdd) { sylvan_protect(&bdd); }
    BddMap() : bdd(sylvan_map_empty()) { sylvan_protect(&bdd); }
    ~BddMap() { sylvan_unprotect(&bdd); }

    BddMap(uint32_t key_variable, const Bdd value);

    BddMap operator+(const Bdd& other) const;
    BddMap& operator+=(const Bdd& other);
    BddMap operator-(const Bdd& other) const;
    BddMap& operator-=(const Bdd& other);

    /**
     * @brief Adds a key-value pair to the map
     */
    void put(uint32_t key, Bdd value);

    /**
     * @brief Removes a key-value pair from the map
     */
    void removeKey(uint32_t key);

    /**
     * @brief Returns the number of key-value pairs in this map
     */
    size_t size() const;

    /**
     * @brief Returns non-zero when this map is empty
     */
    bool isEmpty() const;
};

class MtbddMap;

class Mtbdd {
    friend class Sylvan;
    friend class MtbddMap;

public:
    Mtbdd() { mtbdd = sylvan_false; mtbdd_protect(&mtbdd); }
    Mtbdd(const MTBDD from) : mtbdd(from) { mtbdd_protect(&mtbdd); }
    Mtbdd(const Mtbdd &from) : mtbdd(from.mtbdd) { mtbdd_protect(&mtbdd); }
    Mtbdd(const Bdd &from) : mtbdd(from.bdd) { mtbdd_protect(&mtbdd); }
    ~Mtbdd() { mtbdd_unprotect(&mtbdd); }

    /**
     * @brief Creates a Mtbdd leaf representing the int64 value <value>
     */
    static Mtbdd int64Terminal(int64_t value);

    /**
     * @brief Creates a Mtbdd leaf representing the floating-point value <value>
     */
    static Mtbdd doubleTerminal(double value);

    /**
     * @brief Creates a Mtbdd leaf representing the fraction value <nominator>/<denominator>
     * Internally, Sylvan uses 32-bit values and reports overflows to stderr.
     */
    static Mtbdd fractionTerminal(int64_t nominator, uint64_t denominator);

    /**
     * @brief Creates a Mtbdd leaf of type <type> holding value <value>
     * This is useful for custom Mtbdd types.
     */
    static Mtbdd terminal(uint32_t type, uint64_t value);

    /**
     * @brief Creates a Boolean Mtbdd representing jsut the variable index in its positive form
     * The variable index must be 0<=index<=2^23 (Sylvan uses 24 bits internally)
     */
    static Mtbdd mtbddVar(uint32_t variable);

    /**
     * @brief Returns the Boolean Mtbdd representing "True"
     */
    static Mtbdd mtbddOne();

    /**
     * @brief Returns the Boolean Mtbdd representing "False"
     */
    static Mtbdd mtbddZero();

    /**
     * @brief Returns the Mtbdd representing a cube of variables, according to the given values.
     * @param variables the variables that will be in the cube in their positive or negative form
     * @param values a character array describing how the variables will appear in the result
     * @param terminal the leaf of the cube
     * The length of string must be equal to the number of variables in the cube.
     * For every ith char in string, if it is 0, the corresponding variable will appear in its negative form,
     * if it is 1, it will appear in its positive form, and if it is 2, it will appear as "any", thus it will
     * be skipped.
     */
    static Mtbdd mtbddCube(const BddSet &variables, unsigned char *values, const Mtbdd &terminal);

     /**
     * @brief Returns the Mtbdd representing a cube of variables, according to the given values.
     * @param variables the variables that will be in the cube in their positive or negative form
     * @param values a character array describing how the variables will appear in the result
     * @param terminal the leaf of the cube
     * The length of string must be equal to the number of variables in the cube.
     * For every ith char in string, if it is 0, the corresponding variable will appear in its negative form,
     * if it is 1, it will appear in its positive form, and if it is 2, it will appear as "any", thus it will
     * be skipped.
     */
    static Mtbdd mtbddCube(const BddSet &variables, std::vector<uint8_t> values, const Mtbdd &terminal);

    bool operator==(const Mtbdd& other) const;
    bool operator!=(const Mtbdd& other) const;
    Mtbdd& operator=(const Mtbdd& right);
    Mtbdd operator!() const;
    Mtbdd operator~() const;
    Mtbdd operator*(const Mtbdd& other) const;
    Mtbdd& operator*=(const Mtbdd& other);
    Mtbdd operator+(const Mtbdd& other) const;
    Mtbdd& operator+=(const Mtbdd& other);
    Mtbdd operator-(const Mtbdd& other) const;
    Mtbdd& operator-=(const Mtbdd& other);

    // not implemented (compared to Bdd): <=, >=, <, >, &, &=, |, |=, ^, ^=

    /**
     * @brief Returns non-zero if this Mtbdd is a leaf
     */
    bool isTerminal() const;

    /**
     * @brief Returns non-zero if this Mtbdd is a leaf
     */
    bool isLeaf() const;

    /**
     * @brief Returns non-zero if this Mtbdd is mtbddOne()
     */
    bool isOne() const;

    /**
     * @brief Returns non-zero if this Mtbdd is mtbddZero()
     */
    bool isZero() const;

    /**
     * @brief Returns the top variable index of this Mtbdd (the variable in the root node)
     */
    uint32_t TopVar() const;

    /**
     * @brief Follows the high edge ("then") of the root node of this Mtbdd
     */
    Mtbdd Then() const;

    /**
     * @brief Follows the low edge ("else") of the root node of this Mtbdd
     */
    Mtbdd Else() const;

    /**
     * @brief Returns the negation of the MTBDD (every terminal negative)
     * Do not use this for Boolean MTBDDs, only for Integer/Double/Fraction MTBDDs.
     */
    Mtbdd Negate() const;

    /**
     * @brief Applies the binary operation <op>
     */
    Mtbdd Apply(const Mtbdd &other, mtbdd_apply_op op) const;

    /**
     * @brief Applies the unary operation <op> with parameter <param>
     */
    Mtbdd UApply(mtbdd_uapply_op op, size_t param) const;

    /**
     * @brief Computers the abstraction on variables <variables> using operator <op>.
     * See also: AbstractPlus, AbstractTimes, AbstractMin, AbstractMax
     */
    Mtbdd Abstract(const BddSet &variables, mtbdd_abstract_op op) const;

    /**
     * @brief Computes if f then g else h
     * This Mtbdd must be a Boolean Mtbdd
     */
    Mtbdd Ite(const Mtbdd &g, const Mtbdd &h) const;

    /**
     * @brief Computes f + g
     */
    Mtbdd Plus(const Mtbdd &other) const;

    /**
     * @brief Computes f * g
     */
    Mtbdd Times(const Mtbdd &other) const;

    /**
     * @brief Computes min(f, g)
     */
    Mtbdd Min(const Mtbdd &other) const;

    /**
     * @brief Computes max(f, g)
     */
    Mtbdd Max(const Mtbdd &other) const;

    /**
     * @brief Computes abstraction by summation (existential quantification)
     */
    Mtbdd AbstractPlus(const BddSet &variables) const;

    /**
     * @brief Computes abstraction by multiplication (universal quantification)
     */
    Mtbdd AbstractTimes(const BddSet &variables) const;

    /**
     * @brief Computes abstraction by minimum
     */
    Mtbdd AbstractMin(const BddSet &variables) const;

    /**
     * @brief Computes abstraction by maximum
     */
    Mtbdd AbstractMax(const BddSet &variables) const;

    /**
     * @brief Computes abstraction by summation of f \times g
     */
    Mtbdd AndExists(const Mtbdd &other, const BddSet &variables) const;

    /**
     * @brief Convert floating-point/fraction Mtbdd to a Boolean Mtbdd, leaf >= value ? true : false
     */
    Mtbdd MtbddThreshold(double value) const;

    /**
     * @brief Convert floating-point/fraction Mtbdd to a Boolean Mtbdd, leaf > value ? true : false
     */
    Mtbdd MtbddStrictThreshold(double value) const;

    /**
     * @brief Convert floating-point/fraction Mtbdd to a Boolean Mtbdd, leaf >= value ? true : false
     * Same as MtbddThreshold (Bdd = Boolean Mtbdd)
     */
    Bdd BddThreshold(double value) const;

    /**
     * @brief Convert floating-point/fraction Mtbdd to a Boolean Mtbdd, leaf > value ? true : false
     * Same as MtbddStrictThreshold (Bdd = Boolean Mtbdd)
     */
    Bdd BddStrictThreshold(double value) const;

    /**
     * @brief Computes the support of a Mtbdd.
     */
    Mtbdd Support() const;

    /**
     * @brief Gets the MTBDD of this Mtbdd (for C functions)
     */
    MTBDD GetMTBDD() const;

    /**
     * @brief Functional composition. Whenever a variable v in the map m is found in the MTBDD,
     *        it is substituted by the associated function (which should be a Boolean MTBDD)
     * You can also use this function to implement variable reordering.
     */
    Mtbdd Compose(MtbddMap &m) const;

    /**
     * @brief Substitute all variables in the array from by the corresponding variables in to.
     */
    Mtbdd Permute(const std::vector<uint32_t>& from, const std::vector<uint32_t>& to) const;

    /**
     * @brief Compute the number of satisfying variable assignments, using variables in cube.
     */
    double SatCount(const BddSet &variables) const;

    /**
     * @brief Compute the number of satisfying variable assignments, using the given number of variables.
     */
    double SatCount(const size_t nvars) const;

    /**
     * @brief Gets the number of nodes in this Bdd. Not thread-safe!
     */
    size_t NodeCount() const;

    /////////////////////
    // ADDED BY STORM
    /////////////////////

    // Functions that operate on all or standard Mtbdds.
    Bdd NotZero() const;
    size_t CountLeaves() const;
    double NonZeroCount(size_t variableCount) const;
    bool isValid() const;
    void PrintDot(FILE *out) const;
    std::string GetShaHash() const;
    void PrintText(FILE *out) const;

    Mtbdd Minus(const Mtbdd &other) const;
    Mtbdd Divide(const Mtbdd &other) const;

    Bdd Equals(const Mtbdd& other) const;
    Bdd Less(const Mtbdd& other) const;
    Bdd LessOrEqual(const Mtbdd& other) const;

    Bdd AbstractMinRepresentative(const BddSet &variables) const;
    Bdd AbstractMaxRepresentative(const BddSet &variables) const;

    Mtbdd Pow(const Mtbdd& other) const;
    Mtbdd Mod(const Mtbdd& other) const;
    Mtbdd Logxy(const Mtbdd& other) const;

    Mtbdd Floor() const;
    Mtbdd Ceil() const;
    Mtbdd Minimum() const;
    Mtbdd Maximum() const;

    bool EqualNorm(const Mtbdd& other, double epsilon) const;
    bool EqualNormRel(const Mtbdd& other, double epsilon) const;

    Mtbdd SharpenKwekMehlhorn(size_t precision) const;

    Mtbdd ToRationalNumber() const;

    // Functions that operate on Mtbdds over rational numbers.
    static Mtbdd stormRationalNumberTerminal(storm::RationalNumber const& value);

    Bdd EqualsRN(const Mtbdd& other) const;
    Bdd LessRN(const Mtbdd& other) const;
    Bdd LessOrEqualRN(const Mtbdd& other) const;

    Mtbdd MinRN(const Mtbdd& other) const;
    Mtbdd MaxRN(const Mtbdd& other) const;

    Mtbdd PlusRN(const Mtbdd &other) const;
    Mtbdd MinusRN(const Mtbdd &other) const;
    Mtbdd TimesRN(const Mtbdd &other) const;
    Mtbdd DivideRN(const Mtbdd &other) const;

    Mtbdd FloorRN() const;
    Mtbdd CeilRN() const;
    Mtbdd PowRN(const Mtbdd& other) const;
    Mtbdd ModRN(const Mtbdd& other) const;
    Mtbdd MinimumRN() const;
    Mtbdd MaximumRN() const;

    Mtbdd AndExistsRN(const Mtbdd &other, const BddSet &variables) const;
    Mtbdd AbstractPlusRN(const BddSet &variables) const;
    Mtbdd AbstractMinRN(const BddSet &variables) const;
    Mtbdd AbstractMaxRN(const BddSet &variables) const;

    Bdd AbstractMinRepresentativeRN(const BddSet &variables) const;
    Bdd AbstractMaxRepresentativeRN(const BddSet &variables) const;

    Bdd BddThresholdRN(storm::RationalNumber const& rn) const;
    Bdd BddStrictThresholdRN(storm::RationalNumber const& rn) const;

    bool EqualNormRN(const Mtbdd& other, storm::RationalNumber const& epsilon) const;
    bool EqualNormRelRN(const Mtbdd& other, storm::RationalNumber const& epsilon) const;

    Mtbdd ToDoubleRN() const;

    // Functions that operate on Mtbdds over rational functions.
    static Mtbdd stormRationalFunctionTerminal(storm::RationalFunction const& value);

    Bdd EqualsRF(const Mtbdd& other) const;
    Bdd LessRF(const Mtbdd& other) const;
    Bdd LessOrEqualRF(const Mtbdd& other) const;

    Mtbdd MinRF(const Mtbdd& other) const;
    Mtbdd MaxRF(const Mtbdd& other) const;

    Mtbdd PlusRF(const Mtbdd &other) const;
    Mtbdd MinusRF(const Mtbdd &other) const;
    Mtbdd TimesRF(const Mtbdd &other) const;
    Mtbdd DivideRF(const Mtbdd &other) const;

    Mtbdd FloorRF() const;
    Mtbdd CeilRF() const;
    Mtbdd PowRF(const Mtbdd& other) const;
    Mtbdd MinimumRF() const;
    Mtbdd MaximumRF() const;

    Mtbdd AndExistsRF(const Mtbdd &other, const BddSet &variables) const;
    Mtbdd AbstractPlusRF(const BddSet &variables) const;
    Mtbdd AbstractMinRF(const BddSet &variables) const;
    Mtbdd AbstractMaxRF(const BddSet &variables) const;

    Bdd BddThresholdRF(storm::RationalFunction const& rf) const;
    Bdd BddStrictThresholdRF(storm::RationalFunction const& rf) const;

    bool EqualNormRF(const Mtbdd& other, storm::RationalFunction const& epsilon) const;
    bool EqualNormRelRF(const Mtbdd& other, storm::RationalFunction const& epsilon) const;

    Mtbdd ToDoubleRF() const;

    /////////////////////

private:
    MTBDD mtbdd;
};

class MtbddMap
{
    friend class Mtbdd;
    MTBDD mtbdd;
    MtbddMap(MTBDD from) : mtbdd(from) { mtbdd_protect(&mtbdd); }
    MtbddMap(Mtbdd &from) : mtbdd(from.mtbdd) { mtbdd_protect(&mtbdd); }
public:
    MtbddMap() : mtbdd(mtbdd_map_empty()) { mtbdd_protect(&mtbdd); }
    ~MtbddMap() { mtbdd_unprotect(&mtbdd); }

    MtbddMap(uint32_t key_variable, Mtbdd value);

    MtbddMap operator+(const Mtbdd& other) const;
    MtbddMap& operator+=(const Mtbdd& other);
    MtbddMap operator-(const Mtbdd& other) const;
    MtbddMap& operator-=(const Mtbdd& other);

    /**
     * @brief Adds a key-value pair to the map
     */
    void put(uint32_t key, Mtbdd value);

    /**
     * @brief Removes a key-value pair from the map
     */
    void removeKey(uint32_t key);

    /**
     * @brief Returns the number of key-value pairs in this map
     */
    size_t size();

    /**
     * @brief Returns non-zero when this map is empty
     */
    bool isEmpty();
};

class Sylvan {
public:
    /**
     * @brief Initializes the Sylvan framework, call this only once in your program.
     * @param initialTableSize The initial size of the nodes table. Must be a power of two.
     * @param maxTableSize The maximum size of the nodes table. Must be a power of two.
     * @param initialCacheSize The initial size of the operation cache. Must be a power of two.
     * @param maxCacheSize The maximum size of the operation cache. Must be a power of two.
     */
    static void initPackage(size_t initialTableSize, size_t maxTableSize, size_t initialCacheSize, size_t maxCacheSize);

    /**
     * @brief Set the granularity for the BDD operations.
     * @param granularity determins operation cache behavior; for higher values (2+) it will use the operation cache less often.
     * Values of 3-7 may result in better performance, since occasionally not using the operation cache is fine in practice.
     * A granularity of 1 means that every BDD operation will be cached at every variable level.
     */
    static void setGranularity(int granularity);

    /**
     * @brief Retrieve the granularity for the BDD operations.
     */
    static int getGranularity();

    /**
     * @brief Initializes the BDD module of the Sylvan framework.
     */
    static void initBdd();

    /**
     * @brief Initializes the MTBDD module of the Sylvan framework.
     */
    static void initMtbdd();

    /**
     * @brief Frees all memory in use by Sylvan.
     * Warning: if you have any Bdd objects which are not bddZero() or bddOne() after this, your program may crash!
     */
    static void quitPackage();

    /////////////////////
    // ADDED BY STORM
    /////////////////////

    static void initCustomMtbdd();

    /////////////////////
};

}

#endif
