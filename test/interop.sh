#! /bin/bash 
set -o pipefail

echo "interop testing"

found="../../telehash-js/bin/link.js"
if [ -f "$found" ]; then
  echo "node.js"
  ./bin/test_net_link | "$found"
  ../bin/ping | "$found"
else
  echo "skipping node.js"
fi

# temporarily -disable
found=`which test-net-link-disabled`
if [ -f "$found" ]; then
  echo "go"
  ./bin/test_net_link | "$found"
else
  echo "skipping go"
fi

