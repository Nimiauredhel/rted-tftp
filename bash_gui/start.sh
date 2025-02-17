#!/bin/bash

. ./exe_name.sh

height_big=16
height_mid=12
height_small=8
width_big=32
width_mid=28
width_small=24
menu_height=8
app_title="Untitled TFTP App"
client_title="Untitled TFTP Client"
server_title="Untitled TFTP Server"

read_tag="read"
write_tag="write"
delete_tag="delete"
serve_tag="serve"

peer_ip=127.0.0.1
file_name=
transfer_mode=octet
block_size=512

# brief greeting
dialog --title "$app_title" --infobox "Welcome to $app_title!" $height_small $width_mid
sleep 0.5

# show initial menu - choose operation mode (read, write, delete, serve)
exec 3>&1
op_mode=$(dialog --title "$app_title" --menu "Please select an operation." $height_mid $width_big $menu_height $read_tag "Retrieve File" $write_tag "Send File" $delete_tag "Delete File" $serve_tag "Start Server" 2>&1 1>&3)

case $op_mode in
    $read_tag)
        . ./transfer_op.sh
        ;;
    $write_tag)
        . ./transfer_op.sh
        ;;
    $delete_tag)
        . ./delete_op.sh
        ;;
    $serve_tag)
        . ./serve_op.sh
        ;;
esac

echo Thank you for using $app_title!
