Contributions are welcome.

The public git repository can be found on [github](http://github.com/wraith/wraith github)

## Guides
 * [Github's contribution guide](http://github.com/guides/fork-a-project-and-submit-your-modifications)
 * [Github's pull request guide](http://github.com/guides/pull-requests Github's pull request guide)
 * [Git's Submitting Patches Guide](http://raw.github.com/git/git/master/Documentation/SubmittingPatches)

There's two options for submitting patches:
 * Use github's pull requests to submit patches.
 * Send patches to wraith-patches@botpack.net.
  * [git-format-patch(1)](http://www.kernel.org/pub/software/scm/git/docs/git-format-patch.html) should be used for all patches.

## Guidelines
 * Please stick to the coding style found in the files.
  * No tabs
  * 2 spaces indented for each level
  * Braces on the same line as `if', `else', functions, etc.
 * Avoid non-portable code
 * No ASM
 * Comment all changes which are not clear
 * No change should introduce warnings into the normal or debug compiling.
 * Use strlcpy,strlcat,snprintf,strdup
 * Do not allocate large buffers on the stack (char x[4096] = "")
 * Avoid excessive use of strlen(), cache your values when possible.
 * Use very little C++ additions. (Adds too much overhead since most of the code is using c stdlib)
  * Avoid all STL.
  * Do not use exceptions
  * No RTTI
 * Communicate with other developers in #wraith on EFnet.
