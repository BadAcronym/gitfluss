Status:
+ Linux only for now, win32 needs a small bit of porting
+ You can configure a list of paths in the configuration file. `~` and `$HOME` will be
resolved, all other paths must be absolute for now.
+ You can also configure a list of authors to consider (by email)
+ You can choose a colour (red is default):

The configuration file location is `~/.config/gitfluss/.conf`. Gitfluss will look first
in the current working directory for a `.conf` file and then in the config directory.

Example configuration:
```
author: terribleacronym@gmail.com
author: another_author@workplace.org
colour: red
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

![red theme](screenshots/red.jpg)

other avaiable colours are `green`, `blue`, `yellow`, and `purple`.

![green theme](screenshots/green.jpg)
![blue theme](screenshots/blue.jpg)
![yellow theme](screenshots/yellow.jpg)

with `info: true`, you can see some additional commit history information.

![extra info](screenshots/info.jpg)

With `character: `, you can actually change the character that's being used to print.

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
