#! /bin/bash

cd $( dirname $0 )

id=i-066a53311a02b0fa0

case $1 in
    "id")
        echo ${id}
        ;;
    "ssh")
        pubip=$( ./nfs-instance.sh pubip )

        ssh -i /home/thorpedoes/.ssh/id_rsa jothor@${pubip}
        ;;
    "pubip")
        ip=$( aws ec2 describe-instances --filter Name=instance-id,Values=${id} --query Reservations[*].Instances[*].PublicIpAddress --output text )

        if [[ -z ${ip} ]]; then
            echo "No IP address found. Check if instance is running"
            exit
        fi
        echo ${ip}
        ;;
    "start")
        aws ec2 start-instances --instance-ids ${id}
        ;;
    "stop")
        aws ec2 stop-instances --instance-ids ${id}
        ;;
    "reboot")
        aws ec2 reboot-instances --instance-ids ${id}
        echo "Rebooting NFS Server..."
        ;;
    "state")
        aws ec2 describe-instances --filter Name=instance-id,Values=${id} --query Reservations[*].Instances[*].State --output text
        ;;
    *)
        echo "Unrecognzied option"
        ;;
esac
