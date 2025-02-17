dialog --title "$client_title" --infobox "Selected $op_mode!" $height_small $width_small
sleep 0.5

# get arguments
exec 3>&1
value=$(dialog --title "$client_title" --form "Who should delete what?" $height_big $width_big $menu_height \
    "Peer Address (IPv4):" 1 1 "$peer_ip" 1 12 16 16 \
    "File Name:" 2 1 "$file_name" 2 12 32 255 \
    2>&1 1>&3)

peer_ip=$(echo "$value" | sed -n 1p)
file_name=$(echo "$value" | sed -n 2p)

./$exe_name $op_mode $peer_ip $file_name
