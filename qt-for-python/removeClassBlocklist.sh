#!/bin/bash

BLOCKLIST_FILE=/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/qt-for-python/qgis/core/classBlockList.txt

while true;
do

  current_class=$(head -n 1 $BLOCKLIST_FILE)
  echo " $current_class..."

  if [[ $current_class == "QMetaTypeId" ]]; then
    echo "FIN!!"
    exit 1
  fi


  echo "$(tail -n +2 $BLOCKLIST_FILE)" > $BLOCKLIST_FILE

  ninja update_pyside_bindings && pushd .. &> /dev/null && git apply ./qt-for-python/qgis/core/ts.patch &&  popd &> /dev/null && cmake . && ninja pycore

  if [ $? -eq 0 ] ; then
    echo " $current_class: OK"
  else
    echo " $current_class: NOK"
    echo $current_class >> $BLOCKLIST_FILE
  fi

done
