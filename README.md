# fim
This is a tool to keep track of the folder integrity. It recursively creates the md5 hash for all the files in the path and
stores it under the ".fim" directory. 

Currently init, add and status are the only three commands available. "fim init" to ininitalize the empty repository. "fim add" can be used to add the files to the repository. It can also be used to add the modified files as well.
"fim status" outputs all the modified and the untracked files in the current repository.


Dependencies:
1. openssl: This tool uses openssl for the md5 hash impelmentation.


Install instructions:
1. Clone the project to a directory.
2. run "make". On successful build, fim should be available in the newly created bin/ directory.

Future work:
1. Adding the ignore list.
2. Better console output.
