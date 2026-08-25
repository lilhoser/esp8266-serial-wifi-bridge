# Security

This bridge deliberately exposes an unauthenticated, unencrypted raw TCP
listener on port 23. It must not be exposed directly to the public Internet.
Use an isolated local network, firewall rules, and a separately authenticated
VPN or tunnel when remote access is required.

Do not submit Wi-Fi passwords, EEPROM dumps, full flash captures, private IP
inventories, or prior-owner data in public issues. Report firmware security
problems through GitHub's private vulnerability reporting feature if enabled
for the repository.
