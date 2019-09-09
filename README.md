# Netdata plugins

You just have reached a small collection of [netdata](https://github.com/netdata/netdata) external plugins for [daemontools](http://cr.yp.to/daemontools.html) data monitoring gathering, [qmail](http://cr.yp.to/qmail.html) and [qmail-scanner](http://toribio.apollinare.org/qmail-scanner/).

## svstat.plugin

`svstat.plugin` is a netdata external plugin. It detects presence of a [daemontools](http://cr.yp.to/daemontools.html) by changing working directory to `/service`. The plugin collects uptime or downtime, respectively, in positive or negative number of seconds since last change of the service. The information is gathered from `supervise/status` file in similar manner as [svstat](http://cr.yp.to/daemontools/svstat.html) does, however, the file is accessible only for root by default (This is feature of [supervise](http://cr.yp.to/daemontools/supervise.html) program), therefore `svstat.plugin` has to have `suid` flag set or `CAP_DAC_READ_SEARCH` capability on linux.

The plugin skips all subdirectories starting with `.` character.

## qmail.plugin

`qmail.plugin` is a netdata external plugin. It detects **qmail** presence by checking `/var/log/qmail` directory existence and there it locates all subdirectories containing `smtp` or `send` in theirs name and prepares data collector for each one of them.

It skips all directories starting with `.` character.

**For smtp it collects**:

1. connection with status `ok` or `deny`,
2. average number of connections,
3. end statuses for `0`, `256`, `25600` or *other* value,
4. connection via SMTP protocol type `SMTP` or `ESMTPS`,
5. usage of TLS protocol version `TLS1`, `TLS_1`, `TLS_1.1`, `TLS_1.2`, `TLS_1.3` or *unknown*.

**For send it collects**:

1. number of `start delivery` and `end msg`,
2. number of delivery `success`, `failure` or `deferral`.

This plugin is currently Linux specific.

## scanner.plugin

`scanner.plugin` is a netdata external plugin for monitoring of **scannerd**, which is proprietary software with a log output similar to [qmail-scanner](http://toribio.apollinare.org/qmail-scanner/). It detects **scannerd** presence by locating all directories with `scannerd` substring in its name inside `/var/log` directory and prepares a data collector for each one of them. The plugin is deactivated if there is no such directory.

It skips all directories starting with `.` character.

**It collects**:

1. Emails with status `Clear`, `CLAMDSCAN`, `SPAM-TAGGED`, `SPAM-REJECTED` and `SPAM-DELETED`
2. Spam Cache hits
3. Antivirus Cache hits

The [qmail-scanner](http://toribio.apollinare.org/qmail-scanner/) does not measure _Spam Cache hits_ and _Antivirus Cache hist_, but the collector should work for it either. However, it was not tested.

This plugin is currently Linux specific.

## Configuration

All plugins are configured via [netdata.conf](https://github.com/netdata/netdata/tree/master/collectors/plugins.d#configuration). For example, user may wish to change granularity of data gathering by `svstat.plugin` to 10 seconds:

```cfg
[plugin:svstat]
	update every = 10
	# command options =
```

All plugins accept value from `update every` parameter as a first argument (1 second by default). The second optional argument, from `command options` parameter, is a path to a directory, which plugin tries to set as a working directory at the beginning of its execution. For example, user may wish to set default path of a daemontools service directory to `/run/service` (rather than default `/service`) for `svstat.plugin`:

```cfg
[plugin:svstat]
	update every = 10
	command options = /run/service
```

### Plugin restart

It is possible to restart service by sending signal `QUIT`, `TERM` or `INT` (with command `pkill qmail.plugin` for example) and `qmail.plugin` quits successfully
It will be started by `netdata` again.
This may be wanted if the plugin have been updated or new log directory have been introduced.
