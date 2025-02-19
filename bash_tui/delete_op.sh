dialog --title "$client_title" --infobox "Selected $op_mode!" 4 32
sleep 0.75

# get arguments
exec 3>&1
value=$(dialog --title "$client_title" --form "Who should delete what?" 10 32 5 \
    "Peer Address (IPv4):" 1 4 "$peer_ip" 2 4 16 16 \
    "File Name:" 3 4 "$file_name" 4 4 32 255 \
    2>&1 1>&3)

dialog --title "$client_title" --infobox "Running..." 4 24

peer_ip=$(echo "$value" | sed -n 1p)
file_name=$(echo "$value" | sed -n 2p)

script -q -c "sleep 0.1; ./$exe_name $op_mode $peer_ip $file_name;" | tee -a $outfile | dialog --progressbox "$client_title" 32 60
return_val=$?
