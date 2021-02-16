---
Copyright: © 2021 SANE Project
SPDX-License-Identifier: CC-BY-SA-4.0
---

# Creating A New `sane-backends` Release

This file summarizes most points to pay attention to when planning for
a new `sane-backends` release.  Content has been checked while working
on `$old_version` and getting ready for `$new_version`, where:

``` sh
old_version=1.0.31
new_version=1.0.32
```

## Timetable

It is easiest to pick a release date well in advance so everyone knows
what to expect.  Ignoring security bug fix releases, `sane-backends`
has been released on a roughly half-yearly schedule since `1.0.28`.

Once you pick a date (and time), say `DT`, the planning is simply a
matter of counting back from there:

 - `$DT -  0 days`: **release** :confetti_ball:
 - `$DT -  7 days`: **code freeze** after which only documentation
   changes are allowed.  As an exception, critical issues, such as
   hardware destroying bugs, compile errors and completely unusable
   backends, may still be addressed.
 - `$DT - 21 days`: **feature freeze** after which only bug fixes and
   documentation changes are allowed.
 - `$DT - 35 days`: **schedule announcement** including the timetable
   and a pointer to the corresponding GitLab milestone.

Feel free to adjust the offsets if that works better.  Also, pinging
on the mailing list well in advance, say two, three months, about a
suitable date for everyone involved is a good idea.

> If you mention time of day, on the mailing list, in issues or merge
> requests, use UTC times and mention that, e.g. 09:00 UTC.  People
> are in time zones all over the place and converting to and from UTC
> should be relatively easy for everyone.  Converting from other
> time zones is generally cumbersome, even without things like DST.

## Schedule Announcement

Create a milestone on GitLab that shows the schedule.  The milestone
can be used to collect issues resolved and merge request merged to
`master` that will be included in the `$new_version` and coordinate
work leading up to the release.

Send an announcement to the `sane-devel` mailing list announcing the
schedule and point to the milestone.

Add closed issues that triggered code changes and merge requests to
the milestone so you get an idea of what has been added, fixed,
changed or removed.  This will serve as the input for the NEWS file,
together with `git log` and `git diff` outputs.

## Feature Freeze

New backends, support for new models and new bells and whistles for
existing backends (and frontends) are no longer allowed so this is a
good time to cut a `release/$new_version` branch from the latest
public `master` and publish it on GitLab.

Notify `sane-devel` of the Feature Freeze and point out that merge
requests that have to be included in the upcoming release need to be
targeted at `release/$new_version`.  Anything else can go to `master`
as usual.

For backends added since the `$old_version`, make sure that its
`.desc` file includes a `:new :yes` near the top.  You can find such
backends from the list of added files with:

``` sh
git ls-files -- backend | while read f; do
  git log --follow --diff-filter=A --find-renames=40% \
          --format="%ai  $f" $old_version..release/$new_version -- "$f"
done | cat
```

## Code Freeze

Code changes are no longer allowed, bar exceptional circumstances, so
now is a good time to sync the `po/*.po` files in the repository for
translators.

Announce the Code Freeze on `sane-devel` and invite translators to
contribute their updates.

Start creating the `NEWS` file section for the `$new_version`.  You
should now have a list of issues and merge requests to help you get
started.  The following commands may be helpful as well

``` sh
git diff --stat $old_version..release/$new_version | sort -k3 -n
                        # sorted list of heavily modified files
git log $old_version..release/$new_version
git diff $old_version..release/$new_version    # nitty-gritty details
```

> Note that `po/*.po` files normally see quite a lot of changes due to
> the inclusion of source code line numbers.

Occasionally, you may notice changes that have not been documented,
either in a `.desc` file or a manual page.  Now is a good time to
rectify the omission.

Happy that `NEWS` covers everything?  Then

``` sh
git commit NEWS
git push origin release/$new_version
```

on the day of the release.

## Release

Once `release/$new_version` contains everything that should go in,
including the changes to the `NEWS` file, releasing `sane-backends` is
as easy as pushing a tag and clicking a web UI button.  GitLab CI/CD
takes care of the rest.

``` sh
git tag -a -s $new_version -m Release
git push --tags origin release/$new_release
```

The final job in the release pipeline that is triggered by the above
is a manual job.  You have to press a button in the web UI.  However,
before you do so, create a Personal Access Token (with `api` scope) in
your own GitLab account's `Settings` > [`Access Tokens`][] and use its
value to set the `PRIVATE_TOKEN` variable for the `upload` job in the
`Release` stage.  You need to set this on the page that triggers the
`upload` job.

 [`Access Tokens`]: https://gitlab.com/-/profile/personal_access_tokens
 [`CI/CD`]: https://gitlab.com/sane-project/backends/-/settings/ci_cd

### Updating The Website

After the release artifacts, i.e. the source tarball, have hit the
GitLab [Release][] tab, grab the source tarball to create updated
lists of supported devices and HTML manual pages for the website.

With the `$new_version`'s source tarball:

``` sh
tar xaf sane-backends-$new_version.tar.gz@
cd sane-backends-$new_version
./configure
make -C lib
make -C sanei
make -C doc html-pages
LANG=C make -C doc html-man
```

The last command assumes you have `man2html` in your `$PATH`.  There
are various versions of this command but `make` assumes you are using
the version from one of:

- https://savannah.nongnu.org/projects/man2html/
- https://web.archive.org/web/20100611002649/http://hydra.nac.uci.edu/indiv/ehood/tar/man2html3.0.1.tar.gz

Using anything else is asking for trouble.

> See also #261.

With the various HTML pages generated in `sane-backends-$new_version`,
check out the latest code of the sane-project/website and:

``` sh
cd website
rm man/*
cp .../sane-backends-$new_version/doc/*.[1578].html man/
git add man/
git mv sane-backends.html sane-backends-$old_version.html
cp .../sane-backends-$new_version/doc/sane-{backends,mfgs}.html .
git add sane-{backends,mfgs}.html
```

Next, add a hyperlink to the `$old_version`'s file in
`sane-supported-devices.html` and add an entry for the new release to
`index.html`.

Finally

``` sh
git add sane-supported-devices.html index.html
git commit -m "Update for sane-backends-$new_version release"
git push
```

The push will trigger a GitLab CI/CD pipeline that will update the
website.  Make sure it succeeds (see sane-project/website#33 for one
reason it might fail).

 [Release]: https://gitlab.com/sane-project/backends/-/releases

### Mailing List Announcement

Once the website has been updated successfully, announce the release
on the `sane-announce` mailing list (and Cc: `sane-devel`).  You may
want to ping the `sane-announce` list's moderator (@kitno455) to get
your post approved sooner rather than later.

## Post-Release

With the release all done, there are still a few finishing touches that need taking care of*

* merge `release/$new_version` to current `master`
* remove the `:new` tag from all `doc/descriptions*/*.desc` files
* add a new `UNRELEASED` section at top of the `NEWS` file
* update this file!
* and get those changes on the `master` branch

That's All Folks!
