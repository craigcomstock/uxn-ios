set -x
set -e

cd uxn
for f in $(find . -name '*.tal'); do
  echo $f
  if ! (echo $f | grep library) && ! (echo $f | grep assets); then
    d=$(dirname $f)
    mkdir -p ../roms/$d
    rom=$(basename $f .tal).rom
    bin/uxnasm $f ../roms/$d/$rom
  fi
done
cd -
