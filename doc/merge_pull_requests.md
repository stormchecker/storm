The following steps should be performed to integrate pull requests from GitHub.

1. After a pull request is opened, some automatic build checks should be performed by GitHub Actions.
   Failures of these checks should be fixed.

2. Manually review the pull request on GitHub and suggest improvements if necessary.
   In particular make sure:
   * No unnecessary files were committed (for example build artifacts, etc.)
   * No remains from the development are present (for example debug output, hackish workarounds, etc.)
   * ...

3. Integrate the pull request via GitHub, preferably by *rebase and merge*.

4. Optional (if not done already): add the GitHub repository as another remote for your local copy of the internal repository:
   ```console
   git remote add github https://github.com/stormchecker/storm.git
   ```
   
5. Fetch the current GitHub master:
   ```console
   git fetch github
   ```

6. Make sure to be on the (internal) master:
   ```console
   git checkout master
   ```

7. Rebase the changes of GitHub onto the (internal) master:
   ```console
   git rebase github/master
   ```

8. Check that Storm builds successfully and everything works as expected.

9. Push the changes into the internal repository:
   ```console
   git push origin
   ```
