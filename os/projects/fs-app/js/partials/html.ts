module html {
 export var filesDirectoryBreadcrumbActive = ["<li class=\"active\" data-path=\"<%= path %>\">","    <%= name %>","</li>",""].join("\n");
export var filesDirectoryBreadcrumb = ["<li>","    <a data-path=\"<%= path %>\"><%= name %></a>","</li>",""].join("\n");
export var filesTreeDir = ["<li>","    <i class=\"fa fa-folder-o\"></i>","    <a data-path=\"<%= path %>\"><%= name %></a>","</li>",""].join("\n");
export var filesTreeFile = ["<li data-path=\"<%= path %>\">","    <i class=\"fa fa-file-word-o\"></i>","    <%= name %>","</li>",""].join("\n");
export var filesTreeSubtree = ["<li>","    <i class=\"fa fa-folder-open-o\"></i>","    <a data-path=\"<%= path %>\"><%= name %></a>","</li>","<li class=\"files-tree-sub\">","    <ul class=\"files-tree\">","        <%= sub_tree %>","    </ul>","</li>",""].join("\n"); 
}
