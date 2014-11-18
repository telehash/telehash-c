#! /bin/bash 
set -o pipefail

echo "interop testing"

found="../telehash-js/bin/link.js"
if [ -f "$found" ]; then
  echo "node.js"
  ./bin/test_net_link | "$found"
else
  echo "skipping node.js"
fi

found=`which test-net-link`
if [ -f "$found" ]; then
  echo "go"
  ./bin/test_net_link | "$found"
else
  echo "skipping go"
fi

