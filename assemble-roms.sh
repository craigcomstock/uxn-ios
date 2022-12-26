set -x
set -e
cd uxn/projects/examples
pwd
for f in $(find . -name '*.tal'); do
  echo $f
  d=$(dirname $f)
  mkdir -p ../../../roms/$d
  rom=$(basename $f .tal).rom
  ../../bin/uxnasm $f ../../../roms/$d/$rom
done
#uxn/bin/uxnasm uxn/projects/examples/devices/screen.tal roms/devices/screen.rom
cd -
pwd
