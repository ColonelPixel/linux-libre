GNU Linux-libre: <https://www.fsfla.org/ikiwiki/selibre/linux-libre/>

This repository contains the scripts used to create GNU Linux-libre 
and the source code resulting from that modification. Each version is 
signed and tagged, allowing for easy access and verification.

Changelogs describing the differences compared to upstream Linux and 
GPG signatures for the uncompressed tarballs are accessible as git 
notes:

    git fetch origin refs/notes/*:refs/notes/*

And then:

    git notes --ref refs/notes/signatures/tar show v4.18.5-gnu

or

    git notes --ref refs/notes/changelog show v4.18.5-gnu

You can redistribute and/or modify this file under the terms 
of the GNU General Public License as published by the Free Software 
Foundation; either version 2 of the License, or (at your option) any 
later version.