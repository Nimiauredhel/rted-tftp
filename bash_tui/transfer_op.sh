dialog --title "$client_title" --infobox "Selected $op_mode!" 4 32
sleep 0.5

# get arguments
exec 3>&1
value=$(dialog --title "$client_title" --form "Tell me more." 16 32 8 \
    "Peer Address (IPv4):" 1 4 "$peer_ip" 2 4 16 16 \
    "File Name:" 3 4 "$file_name" 4 4 32 255 \
    "Transfer Mode:" 5 4 "$transfer_mode" 6 4 0 8 \
    "Block Size:" 7 4 "$block_size" 8 4 8 8 \
    2>&1 1>&3)

peer_ip=$(echo "$value" | sed -n 1p)
file_name=$(echo "$value" | sed -n 2p)
transfer_mode=octet
block_size=$(echo "$value" | sed -n 3p)

dialog --title "$client_title" --infobox "Running..." 4 24

script -q -c "sleep 0.1; ./$exe_name $op_mode $peer_ip $file_name $transfer_mode $block_size;" | tee -a $outfile | dialog --progressbox "$client_title" 32 60
return_val=$?
