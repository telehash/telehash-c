#! /bin/bash 
set -o pipefail

./bin/test_net_link | ../telehash-js/bin/link.js
