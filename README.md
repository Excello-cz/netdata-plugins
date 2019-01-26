# qmail.plugin

`qmail.plugin` is a netdata external plugin. It detects **qmail** presence by checking `/var/log/qmail` directory existence and there it locates all subdirectories containing `smtp` or `send` in theirs name and prepares data collector for each one of them.

It skips all directories starting with `.` character.

**For smtp it collects**:

1. connection with status `ok` or `deny`,
2. average number of connections,
3. end statuses for `0`, `256`, `25600` or *other* value.

**For send it collects**:

1. number of `start delivery` and `end msg`,
2. number of delivery `success`, `failure` or `deferral`.

This plugin is currently Linux specific.

### Plugin restart

It is possible to restart service by sending signal `QUIT`, `TERM` or `INT` (with command `pkill qmail.plugin` for example) and `qmail.plugin` quits successfully
It will be started by `netdata` again.
This may be wanted if the plugin have been updated or new log directory have been introduced.
