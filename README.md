# callisto

This is the software for linux (or similar systems) needed to be part of the [e-callisto](http://www.e-callisto.org) network.

# Installation (on Debian or Ubuntu)

`apt install callisto`

# Configuration

in /usr/share/callisto/examples you find an example configuration for a station
to be put in /etc/callisto

# Automatic Launch

Easily to do with cronjobs. You might want to add, `crontab -e`
```
@reboot /usr/bin/screen -dmS callisto /usr/sbin/callisto -d
01 * * * * /var/www/callisto/ftpupload.pl
16 * * * * /var/www/callisto/ftpupload.pl
31 * * * * /var/www/callisto/ftpupload.pl
46 * * * * /var/www/callisto/ftpupload.pl
```

# Submit data to E-CALLISTO

If you want to send data, make sure to run callisto with `TZ=UTC`.
You can check your timezone setting with `date` vs `date -u`.

see `examples/ftpupload.pl`
