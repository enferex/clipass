### CliPass: Clipboard password utility

#### What
CliPass is a command line interface tool which reads in data from a file,
~/.config/clipass/entropy and stores a subset of that data into your
clipboard.

#### Why
Longer passwords can be more secure; therefore, more annoying to enter manually.
CliPass will read data from a specified offset, store it into your clipboard,
and that can be used as a password which you can paste.  The clipboard is
cleared after 5 seconds.

The user is responsible for creating the entropy file
~/.config/clipass/entropy.data
The user must make this file read-only by themselves (chmod 400).

CliPass also has the ability to generate entroypy files based on the
/dev/urandom.  All of the data is converted to 7-bit printable ASCII, since it
is common to require passwords within the printable character set.

#### Rant
You are responsible for how you use this tool. Passwords can be a bad form of
securing information; however, it's still common practice.  In an ideal world,
it would be great to eliminate passwords entirely.  But lets at least do better,
e.g., making a long password can help, and that's what clipass does, and also
eliminates the hassle of having to remember the lengthy passwords.

#### Use
 * Create a password entropy file (~/.config/clipass/entropy.data) see the '-g'
 option.
 * Remember just the offset into your entropy file
   (by default all passwords are 64 characters long).
 * ClipAss will automatically copy to your primary clipboard, 64 bytes from the entropy file starting from your specified offset.
 * Paste the contents of your clipboard into whatever password form.
 * ClipAss will automatically clear the clipboard contents.  Or you can just run
 the tool again if you are paranoid (which is a good thing, the paranoia).

#### Bonus
This is a great source to get an idea of how X11 selections work (e.g.,
"clipboard" from an X environment).

#### Build
Simply run `make` to build.

#### Contact
mattdavis9@gmail.com (enferex)
