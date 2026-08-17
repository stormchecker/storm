The following steps should be performed when releasing a new Storm version.
Note that in most cases a simultaneous release of [carl-storm](https://github.com/stormchecker/carl-storm), [Storm](https://github.com/stormchecker/storm) and [stormpy](https://github.com/stormchecker/stormpy/) is preferred.


## Preparations for the release
1. Update the used carl version:
   This should be automatically handled by a CI workflow triggered by new releases in carl-storm.
   To manually update the carl version, update the following locations:
   * `STORM_CARL_GIT_TAG` in `CMakeLists.txt`
   * `carl_tag` in `Dockerfile`, `.github/workflows/Dockerfile.alpine`, `.github/workflows/Dockerfile.archlinux` and `.github/workflows/Dockerfile.release`

2. Update the versions of the dependencies in the [storm-dependencies Dockerfile](https://github.com/stormchecker/docker-storm/blob/main/storm-dependencies/Dockerfile).
   Use the [CI of docker-storm](https://github.com/stormchecker/docker-storm/actions/workflows/dependencies.yml) to create new Docker images.

3. Check that Storm [CI](https://github.com/stormchecker/storm/actions/) builds without errors and all tests are successful.


## Creating the release
At this point, all relevant pull requests should have been merged into Storm and no new commits should be added until the release was successful.

4. Update `CHANGELOG.md`:
   * Set release month
   * Set major changes.
     Use the [automatically generated release notes](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes).
     Alternatively, get all the commits since the `last_tag` by executing:
     ```console
     git log last_tag..HEAD
     ```

5. Set new Storm version in `CMakeList.cmake`.

6. Create a new pull request with the changes of steps 4 and 5.
   When the CI checks are successful, stash and merge the pull request into the master branch.

7. (The tag can also automatically be set in the next step when creating the release on GitHub.)
   Set the new tag in Git, use the flag `-s` to sign the tag.
   ```console
   git tag -a X.Y.Z -m "Storm version X.Y.Z" -s
   git push origin X.Y.Z
   ```
   The new tag should now be visible on [GitHub](https://github.com/stormchecker/storm/tags).

8. [Create a new release](https://github.com/stormchecker/storm/releases/new) on GitHub.
   Create a new tag or use the tag created in the previous step.
   Finishing the release automatically triggers a CI workflow which also
   * updates the `stable` branch
   * creates new Docker containers for both the tag and `stable` branch
   * triggers a PR in stormpy to update the Storm version
   * triggers a PR in Storm to set the development version of Storm again


## After the release
At this point new commits can be added to Storm again.
A natural next step is to [prepare a stormpy release](https://github.com/stormchecker/stormpy/blob/master/doc/checklist_new_release.md).

9. Update the packages for the supported operating systems:
   - macOS: Update the [Homebrew formula](https://github.com/moves-rwth/homebrew-storm):
     - Bump the Storrm and carl-storm versions, and update the checksums
     - After the CI for a PR is successful, add the label `pr-pull`. This automatically creates a new Release with the corresponding homebrew binaries.
   - Archlinux: Update the [stormchecker](https://aur.archlinux.org/packages/stormchecker) package in the AUR.

10. Announce the new Storm version on the [website](http://www.stormchecker.org/news.html).
