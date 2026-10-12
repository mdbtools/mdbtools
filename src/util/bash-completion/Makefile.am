if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = mdb-count mdb-export mdb-hexdump mdb-json mdb-parsecsv mdb-prop mdb-queries mdb-schema mdb-tables mdb-ver
if SQL
  dist_bashcompletion_DATA += mdb-sql
endif
endif
# mdb-import is experimental and not installed, but ship its completion script
EXTRA_DIST = mdb-import
