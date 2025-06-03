systemctl stop carmonitor.service
rm /etc/systemd/system/carmonitor.service
cp carmonitor.service /etc/systemd/system
chmod +x shutdown.sh
systemctl enable carmonitor.service
systemctl start carmonitor.service
