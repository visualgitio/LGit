LGit is an MSSCCI/SCC provider that provides Git support with compatible IDEs.
Such IDEs, to my knowledge, include:

- Visual Studio
- Visual C++
- Visual Basic
- PowerBuilder
- Matlab

I've also heard that some others may be compatible:

- Access
- FrontPage
- Borland IDEs like Delphi

LGit is rough around the edges and only exposes features the IDEs will provide.
These features include:

- Initializing source control ("git init")
- Checking in ("git add" modified, then "git commit")
- Uncheckout ("git checkout HEAD" files)
- Remove ("git rm" files from index, then "git commit")
- Add ("git add" new files not in index, then "git commit")
- History ("git log" selected files)
- Diff ("git diff" for selected files; working tree vs. HEAD)
- Properties and query info ("git status")

The MSSCCI APIs assume a very centralized-style VCS, like SourceSafe or CVS.
As such, some operations don't really map to anything. For example, checking
out isn't supported due to how it doesn't map to Git's worldview. Instead, all
files are marked as checked out. Other features like push/pull, branches, etc.
would be supported via the SccRunScc exposed UI, which isn't ready yet. (Even
with those centralized VCSes, it was expected to shell out to an external UI
for advanced features.)

LGit requires a custom libgit2 fork with patches for Visual C++ 6
compatibility. This is in the process of being cleaned up so it can be
upstreamed.

Please contact calvin@cmpct.info for questions.