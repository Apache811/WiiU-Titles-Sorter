# Titles Sorter settings persistence

User preferences are stored separately from the Wii U Menu account save at:

`fs:/vol/external01/wiiu/apps/TitlesSorter/settings.ini`

The file is a versioned UTF-8-compatible ASCII key/value document. Version 2 stores the sort field, folder placement, folder-content sorting flags,
the excluded Home Menu pages, `autobackup` (default `1`) and
`max_autobackups` (default `10`, accepted range 1–100). Version 1 files remain
readable; missing backup settings receive the new defaults and are upgraded to
version 2 on the next successful settings write. The file is loaded after the startup coordinator has
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
