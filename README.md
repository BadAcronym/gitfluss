Status:
+ Linux only for now, win32 needs a small bit of porting
+ You can configure a list of paths in the configuration file. `~` and `$HOME` will be
resolved, all other paths must be absolute for now.
+ You can also configure a list of authors to consider (by email)
+ You can choose a colour (red is default):

The configuration file location is `~/.config/gitfluss/.conf`. Gitfluss will look first
in the current working directory for a `.conf` file and then in the config directory.

If, instead, command-line arguments are provided, the configuration file will not be
read, for example:

```
gitfluss . --author name@company.com  --colour purple --char ◼ --info
```

Every command-line argument that's not a `colour`, `info` or `author` identifier will be
read as a path. This is the same way the configuration file works. The identifiers
(besides `--info`) need to be followed by the chosen colour or author, else they have no
effect.

Example configuration:
```
author: author@provider.com
author: another_author@workplace.org
colour: purple
character: ◼
info: false
$HOME/repository/puddle
$HOME/repository/imgsurf
$HOME/repository/river2D
$HOME/repository/river2D_mapedit
$HOME/repository/river3D
$HOME/repository/gitfluss
$HOME/repository/bash-collection
```

With `info: false`, you can hide the additional commit history information.

With `author: any`, (or not specifying an author at all) you can omit the author check
and count all commits by all authors in the listed repositories.

With `character: ?`, you can actually change the character that's being used to print.

![example](screenshots/purple.jpg)

The avaiable colours are `red`(default), `green`, `blue`, `cyan`, `yellow` and `purple`.

![red theme](screenshots/red.jpg)
![green theme](screenshots/green.jpg)
![blue theme](screenshots/blue.jpg)
![yellow theme](screenshots/yellow.jpg)

`character: o`
![o](screenshots/o.jpg)

`character: $`
![dollarsign](screenshots/dollarsign.jpg)

`character: █`
![fullblock](screenshots/fullblock.jpg)

If the character is an emoji, the ANSI terminal colours will not be able to take effect.
`character: ☺️`
![smiley](screenshots/smiley.jpg)

Keep in mind that if the character is not monospace, the header will appear to be wrong.
`character: 🪑`
![chair](screenshots/chair.jpg)
