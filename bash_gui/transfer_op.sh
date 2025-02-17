dialog --title "$client_title" --infobox "Selected $op_mode!" $height_small $width_small
sleep 0.5

# get arguments
exec 3>&1
value=$(dialog --title "$client_title" --form "Tell me more." $height_big $width_big $menu_height \
    "Peer Address (IPv4):" 1 1 "$peer_ip" 1 12 16 16 \
    "File Name:" 2 1 "$file_name" 2 12 32 255 \
    "Transfer Mode:" 3 1 "$transfer_mode" 3 12 0 8 \
    "Block Size:" 4 1 "$block_size" 4 12 8 8 \
    2>&1 1>&3)

peer_ip=$(echo "$value" | sed -n 1p)
file_name=$(echo "$value" | sed -n 2p)
transfer_mode=octet
block_size=$(echo "$value" | sed -n 3p)

./$exe_name $op_mode $peer_ip $file_name $transfer_mode $block_size
