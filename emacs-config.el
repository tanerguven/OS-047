(semantic-reset-system-include)
(setq project-include (concat (getenv "PWD") "/include"))
(setq company-c-headers-path-system (list project-include))
(semantic-add-system-include project-include 'c-mode)
