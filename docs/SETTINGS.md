# Titles Sorter settings persistence

User preferences are stored separately from the Wii U Menu account save at:

`fs:/vol/external01/wiiu/apps/TitlesSorter/settings.ini`

The file is a versioned UTF-8-compatible ASCII key/value document. Version 1
stores the sort field, folder placement, folder-content sorting flags and the
excluded Home Menu pages. The file is loaded after the startup coordinator has
created its session and before the interactive UI is entered. This ordering is
intentional: the coordinator resets its session during `Begin()`, so loading
preferences before that call would cause them to be discarded. Missing files
select the application defaults. Invalid or unsupported files are rejected and
defaults are retained. The Home Menu save is never modified for settings
persistence.

Writes happen only after an input that can change an option or after the
excluded-pages keyboard confirms a new expression. The complete new content is
written and flushed to `.tmp` first. The previous file is moved to `.bak` while
the new file is installed; if startup finds the main file missing but a valid
backup exists, the backup is restored. A failed write is logged and does not
block the UI.
