Status:
+ Linux only for now, win32 needs a small bit of porting
+ You can configure a list of paths in the configuration file
+ You can also configure a list of authors to consider (by email)
+ You can choose a colour (red is default):

The configuration file location is `~/.config/gitfluss/.conf`. Gitfluss will look first
in the current working directory for a `.conf` file and then in the config directory.

Example configuration:
```
author: terribleacronym@gmail.com
$HOME/repository/puddle
$HOME/repository/imgsurf
$HOME/repository/river2D
$HOME/repository/river2D_mapedit
$HOME/repository/river3D
$HOME/repository/gitfluss
$HOME/repository/bash-collection
```

The heatmap display is currently under construction, the data being presented is not
truthful (yet). But, it would look like follows:

![red theme](screenshots/red.jpg)
![green theme](screenshots/green.jpg)
