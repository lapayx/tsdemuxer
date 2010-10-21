#!/bin/bash

mkdir -p foo/data/opt/bin
mkdir -p foo/data/opt/share/pshare/www
mkdir -p foo/data/opt/share/pshare/playlists

cp pshare foo/data/opt/bin/
cp -r www/* foo/data/opt/share/pshare/www/
cp -r playlists/* foo/data/opt/share/pshare/playlists/

tar -C ipkg -cz control > foo/control.tar.gz
tar -C foo/data -cz opt > foo/data.tar.gz
echo "2.0" > foo/debian-binary

rm -rf foo/data

tar -C foo -cz ./debian-binary ./data.tar.gz ./control.tar.gz > pshare_0.0.1_mipsel.ipk

rm -rf foo/
