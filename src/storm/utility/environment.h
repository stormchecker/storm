#pragma once

#include <memory>

namespace storm {
class Environment;
}  // namespace storm

namespace storm::utility::environment {

class EnvironmentFactory {
   public:
    virtual ~EnvironmentFactory() = default;

    /*!
     * Creates a new environment with default values.
     *
     * @return The newly created environment.
     */
    virtual storm::Environment create() const;
    virtual std::unique_ptr<EnvironmentFactory> clone() const;
};

class SoundEnvironmentFactory : public EnvironmentFactory {
   public:
    /*!
     * Creates a new environment which enforces sound computations.
     * This is equivalent to the CLI option --sound.
     * In addition, sound solution methods are selected. They are marked as set from default, such that e.g. exact mode can still
     * override them.
     *
     * @return The newly created environment.
     */
    virtual storm::Environment create() const override;
    virtual std::unique_ptr<EnvironmentFactory> clone() const override;
};

/*!
 * Creates a new environment which enforces sound computations (via SoundEnvironmentFactory).
 *
 * @return The newly created environment.
 */
storm::Environment getSoundEnvironment();

class ExactEnvironmentFactory : public EnvironmentFactory {
   public:
    /*!
     * Creates a new environment which enforces exact computations.
     * This is equivalent to the CLI option --exact with respect to the environment.
     * In addition, exact solution methods are selected. They are marked as set from default.
     * Note that the value type (e.g. rational numbers) is chosen when building the model and is not part of the environment.
     *
     * @return The newly created environment.
     */
    virtual storm::Environment create() const override;
    virtual std::unique_ptr<EnvironmentFactory> clone() const override;
};

/*!
 * Creates a new environment which enforces exact computations (via ExactEnvironmentFactory).
 *
 * @return The newly created environment.
 */
storm::Environment getExactEnvironment();

}  // namespace storm::utility::environment
