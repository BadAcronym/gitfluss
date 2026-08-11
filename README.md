![example](screenshots/purple.jpg)

Gitfluss is a neat little cmd git stats view that will pull data from local
repositories, so you can view a unified heatmap across all your projects, whether local
only or cloned from any remote server.

Status:
+ linux + windows versions, windows version has a quirk or two
+ You can configure a list of paths in the configuration file or by passing command-line
  arguments. `~`, `.` and `$HOME` will be resolved, all other paths must be absolute for
  now.
+ You can configure the application to your liking, with author filtering, custom
  colours & characters.

Plans:
+ multi-thread per-repository for even more speedup
+ Allow overwriting percentiles
+ Add more (optional) repository-specific data displays
+ Add more (optional) years to the heatmap display
+ Allow custom RGB colour values

Known issues:
+ Passing non-ascii chars on windows via the cmdline is broken, please use the
  configuration file for that.

Example run with all options enabled:

![example](screenshots/all.jpg)

The configuration file location is `~/.config/gitfluss/.conf`. Gitfluss will look first
in the current working directory for a `.gitflussconf` file and then in the config
directory for a `.conf` file.
On windows, `~` or `$HOME` equates to `$env:USERPROFILE` and the paths are
`~/.config/gitfluss/gitfluss.ini` or locally, `gitfluss.ini`.

If, instead, command-line arguments are provided, the configuration file will not be
read, for example:

```
gitfluss . --author name@company.com --colour purple --char ◼ --info
```

Every command-line argument that's not a `colour`, `info` or `author` identifier will be
read as a path. This is the same way the configuration file works. The identifiers
(besides `--info`) need to be followed by the chosen colour or author, else they have no
effect.

`--profile` will show miscellaneous timings and stats.

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
$HOME/repository/river3D
$HOME/repository/gitfluss
```

Alternatively, lists of authors and files are supported as follows:
configuration file:
```
authorlist: ~/mydir/authorlist_file
repolist: ~/mydir/repolist_file
```

`~/mydir/authorlist_file`:
```
author@provider.com
another_author@workplace.org
...
```

`~/mydir/repolist_file`:
```
$HOME/repository/puddle
$HOME/repository/imgsurf
...
```

comments are supported in all files, preceded with `//` (C-style).

The correspoding heat intensities are calculated based on the `20th`, `50th`, `70th` and
`90th` percentile of your commit count per day, not including days with 0 commits.

The avaiable colours are `red`(default), `green`, `blue`, `cyan`, `yellow` and `purple`.

With `info: false`, you can hide the additional commit history information.

With `author: any`, (or not specifying an author at all) you can omit the author check
and count all commits by all authors in the listed repositories.

With `character: ?`, you can actually change the character that's being used to print:

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

Monochrome support is also built-in, which avoids outputting ANSI colour escape codes to
the terminal. `--mono` in the command-line or `mono: true` in the configuration file
will make gitfluss output in monochrome mode. By default, it will use the full-block
shading characters:

![shades](screenshots/shades.jpg)

But the `--heat0 .`, `--heat1 o` ... / `heat0: .`, `heat1: o` overrides also work to
replace the characters below each percentile:
```
heat0: .
heat1: o
heat2: O
heat3: 0
heat4: @
```

![os](screenshots/os.jpg)

I am not very skilled at ASCII art as you can see, but those who are may happily use
this feature.
