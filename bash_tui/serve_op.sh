#!/bin/bash

dialog --title "$server_title" --infobox "Selected $op_mode!" 4 32
sleep 0.75

script -q -c "sleep 0.1; ./$exe_name $op_mode;" | tee -a $outfile | dialog --progressbox "$server_title" 32 80
