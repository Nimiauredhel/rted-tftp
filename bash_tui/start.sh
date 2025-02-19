#!/bin/bash

. ./exe_name.sh

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

outfile=output-$$
return_val=0

# brief greeting
dialog --title "$app_title" --infobox "Welcome to $app_title!" 4 40
sleep 0.5

touch $outfile
echo "--------" > $outfile
rm $outfile

main_menu()
{
    # show initial menu - choose operation mode (read, write, delete, serve)
    exec 3>&1
    op_mode=$(dialog --title "$app_title" --menu "Please select an operation." 12 32 12 $read_tag "Retrieve File" $write_tag "Send File" $delete_tag "Delete File" $serve_tag "Start Server" 2>&1 1>&3)
}

start_operation()
{
    cancelled=false

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
        *)
            return 255
            ;;
    esac

    echo "
    Press any key to quit." >> $outfile
    dialog --title "$app_title" --infobox "$(tail -n 10 $outfile)" 24 60

    rm $outfile

    read -n 1 -s -r -p ""

    return 0
}

result=0

while [ $result != 255 ]
do
    main_menu
    start_operation
    result=$?
done

echo Goodbye!
