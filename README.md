# fim
This is a tool to keep track of the folder integrity. It recursively creates the md5 hash for all the files in the path and
stores it under the ".fim" directory. 

Currently init, add and status are the only three commands available. "fim init" to ininitalize the empty repository. "fim add" can be used to add the files to the repository. It can also be used to add the modified files as well.
"fim status" outputs all the modified and the untracked files in the current repository. 
