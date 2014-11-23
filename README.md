# ssh-pageant
_An SSH authentication agent for Cygwin/MSYS that links OpenSSH to PuTTY's Pageant._

ssh-pageant is a tiny tool for Windows that allows you to use SSH keys from
[PuTTY]'s Pageant in [Cygwin] and [MSYS] shell environments.

You can use ssh-pageant to automate SSH connections from those shells, which
is useful for services built on top of SSH, like SFTP file transfers or pushing
to secure git repositories.

`ssh-pageant` works like `ssh-agent`, except that it leaves the key storage to
PuTTY's Pageant.  It sets up an authentication socket and prints the environment
variables, which allows OpenSSH connections to use it.


## Installation

The `INSTALL` file describes how to build and install `ssh-pageant` from source,
but the easiest way is to use the readily-available [binary releases]:

1. Download the pre-built [32-bit] or [64-bit] release for Cygwin, or
the [32-bit][32-bit-msys] release for MSYS.

2. Just copy the exe into your PATH and ensure it is executable:

        $ cp ssh-pageant.exe /usr/bin/
        $ chmod 755 /usr/bin/ssh-pageant.exe

3. Optionally, copy the manpage as well:

        $ cp ssh-pageant.1 /usr/share/man/man1/


## Usage

1. Ensure that PuTTY's Pageant is running (and holds your SSH keys).
    * ssh-pageant does not start Pageant itself.
    * Recommended: Add Pageant to your Windows startup/Autostart configuration
      so it is always available.

2. Edit your `~/.bashrc` (or `~/.bash_profile`) to add the following:

        # ssh-pageant
        eval $(/usr/bin/ssh-pageant -r -a "/tmp/.ssh-pageant-$USERNAME")

    To explain:

    * This leverages the `-r`/`--reuse` option (available since 1.3) in
      combination with `-a SOCKET`, which will only start a new daemon if the
      specified path does not accept connections already.  If the socket appears
      to be active, it will just set `SSH_AUTH_SOCK` and exit.

    * The exact path used for `-a` is arbitrary.  The socket will be created
      with only user-accessible permissions, as long as the filesystem is not
      mounted `noacl`, but you may still want to use a more private path than
      shown above on multi-user systems.

    * When using this, the `ssh-pageant` daemon remains to be active (and
      visible in your task manager).  You should not kill the process, since
      open shells might still be using the socket.

    * Using `eval` will set the environment variables in the current shell.
      By default, ssh-pageant outputs sh-style commands.  Use the `-c` option
      for csh-style commands.

You could also rename `ssh-pageant` to `ssh-agent` and then use something like
`keychain` to manage a single instance (the approach of [Charade]), but that is
unnecessary with the `--reuse` option.

It may be possible to share a Cygwin socket with external tools like
[msysgit](http://msysgit.github.io/), if you use a socket path accessible by
both runtimes.  Use `cygpath --windows {path}` to help normalize paths for
system-wide use.

## Options

`ssh-pageant` aims to be compatible with `ssh-agent` options:

    $ ssh-pageant -h
    Usage: ssh-pageant [options] [command [arg ...]]
    Options:
      -h, --help     Show this help.
      -v, --version  Display version information.
      -c             Generate C-shell commands on stdout.
      -s             Generate Bourne shell commands on stdout. (default)
      -k             Kill the current ssh-pageant.
      -d             Enable debug mode.
      -q             Enable quiet mode.
      -a SOCKET      Create socket on a specific path.
      -r, --reuse    Allow to reuse an existing -a SOCKET.
      -t TIME        Limit key lifetime in seconds (not supported by Pageant).


## Known issues

* Pageant is running, but the agent reports `SSH_AGENT_FAILURE`.
    * Fixed in release 1.1.
    * Ensure you have PuTTY Pageant 0.62 or later.
    * Another known workaround is to launch Pageant using `cygstart`.


## Uninstallation

To uninstall, just remove the copied files:

    $ rm /usr/bin/ssh-pageant.exe
    $ rm /usr/share/man/man1/ssh-pageant.1


## Version History

* 2014-11-23: 1.4 - MSYS support and more robust socket paths.
* 2013-06-23: 1.3 - Allow reusing existing sockets via `-r`/`--reuse`.
* 2012-11-24: 1.2 - Mirror the exit status of child processes.
* 2011-06-12: 1.1 - Fixed SID issues.
* 2010-09-20: 1.0 - Initial release.


## Contributions

`ssh-pageant` is considered stable at this point and rarely needs to be updated.

However, in case you encounter any [issues], feel free to create one.  Pull
requests are even more welcome. :)


## Project History

Once upon a time I privately developed a Cygwin terminal based on PuTTY, which
I wanted because I could use Cygwin-native ptys with PuTTY's interface.  As part
of that I also added an `SSH_AUTH_SOCK` shim which talked to Pageant.  Then I
discovered MinTTY, which does the terminal part much better.

The author wasn't interested in including the `SSH_AUTH_SOCK` functionality
though, so instead I split that out into this program, ssh-pageant, and I
finally published the code in April 2009.


## Links

* [PuTTY]: An SSH client for Windows (including the Pageant authentication agent).
* [Cygwin]: A Linux-like environment for Windows.
* [MSYS]: Another Linux-like environment, made to supplement MinGW.
* [OpenSSH]: The SSH client shipped by Cygwin.
* [Charade]: The friendly competition to ssh-pageant.

------------------------------------------------------------------------------
Copyright (C) 2009-2014  Josh Stone  
Licensed under the GNU GPL version 3 or later, http://gnu.org/licenses/gpl.html

This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

See the `COPYING` file for license details.  
Part of ssh-pageant is derived from the PuTTY program, whose original license is
in the file `COPYING.PuTTY`.


[binary releases]: https://github.com/cuviper/ssh-pageant/releases
[32-bit]: https://github.com/cuviper/ssh-pageant/releases/tag/v1.4-prebuilt-cygwin32
[64-bit]: https://github.com/cuviper/ssh-pageant/releases/tag/v1.4-prebuilt-cygwin64
[32-bit-msys]: https://github.com/cuviper/ssh-pageant/releases/tag/v1.4-prebuilt-msys32
[issues]: http://github.com/cuviper/ssh-pageant/issues
[PuTTY]: http://www.chiark.greenend.org.uk/~sgtatham/putty/
[Cygwin]: http://www.cygwin.com/
[MSYS]: http://www.mingw.org/wiki/MSYS
[OpenSSH]: http://www.openssh.com/
[Charade]: http://github.com/wesleyd/charade
