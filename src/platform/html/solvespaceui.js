function isModal() {
    var hasModal = !!document.querySelector('.modal');
    var hasMenuBar = !!document.querySelector('.menubar .selected');
    var hasPopupMenu = !!document.querySelector('.menu.popup');
    var hasEditor = false;
    document.querySelectorAll('.editor').forEach(function(editor) {
        if(editor.style.display == "") {
            hasEditor = true;
        }
    });
    return hasModal || hasMenuBar || hasPopupMenu || hasEditor;
}

/* CSS helpers */
function hasClass(element, className) {
    return element.classList.contains(className);
}
function addClass(element, className) {
    element.classList.add(className);
}
function removeClass(element, className) {
    element.classList.remove(className);
}
function removeClassFromAllChildren(element, className) {
    element.querySelectorAll('.' + className).forEach(function(element) {
        removeClass(element, className);
    })
}

/* Mnemonic helpers */
function setLabelWithMnemonic(element, labelText) {
    var label = document.createElement('span');
    addClass(label, 'label');
    element.appendChild(label);

    var matches = labelText.match('(.*?)&(.)(.*)?');
    if(matches) {
        label.appendChild(document.createTextNode(matches[1]));
        if(matches[2]) {
            var mnemonic = document.createElement('u');
            mnemonic.innerText = matches[2];
            label.appendChild(mnemonic);
            addClass(element, 'mnemonic-Key' + matches[2].toUpperCase());
        }
        if(matches[3]) {
            label.appendChild(document.createTextNode(matches[3]));
        }
    } else {
        label.appendChild(document.createTextNode(labelText))
    }
}

/* Button helpers */
function isButton(element) {
    return hasClass(element, 'button');
}

/* Button DOM traversal helpers */
function getButton(element) {
    if(!element) return;
    if(element.tagName == 'U') {
        element = element.parentElement;
    }
    if(hasClass(element, 'label')) {
        return getButton(element.parentElement);
    } else if(isButton(element)) {
        return element;
    }
}

/* Button behavior */
window.addEventListener('click', function(event) {
    var button = getButton(event.target);
    if(button) {
        button.dispatchEvent(new Event('trigger'));
    }
});

window.addEventListener('keydown', function(event) {
    var selected = document.querySelector('.button.selected');
    if(!selected) return;

    var outSelected, newSelected;
    if(event.key == 'ArrowRight') {
        outSelected = selected;
        newSelected = selected.nextElementSibling;
        if(!newSelected) {
            newSelected = outSelected.parentElement.firstElementChild;
        }
    } else if(event.key == 'ArrowLeft') {
        outSelected = selected;
        newSelected = selected.previousElementSibling;
        if(!newSelected) {
            newSelected = outSelected.parentElement.lastElementChild;
        }
    } else if(event.key == 'Enter') {
        selected.dispatchEvent(new Event('trigger'));
    } else if(event.key == 'Escape' && hasClass(selected, 'default')) {
        selected.dispatchEvent(new Event('trigger'));
    }

    if(outSelected) removeClass(outSelected, 'selected');
    if(newSelected) addClass(newSelected, 'selected');

    event.stopPropagation();
});

/* Editor helpers */
function isEditor(element) {
    return hasClass(element, 'editor');
}

/* Editor DOM traversal helpers */
function getEditor(element) {
    if(!element) return;
    if(isEditor(element)) {
        return element;
    }
}

/* Editor behavior */
window.addEventListener('keydown', function(event) {
    var editor = getEditor(event.target);
    if(editor) {
        if(event.key == 'Enter') {
            editor.dispatchEvent(new Event('trigger'));
        } else if(event.key == 'Escape') {
            editor.style.display = 'none';
        }
        event.stopPropagation();
    }
}, {capture: true});

/* Menu helpers */
function isMenubar(element) {
    return hasClass(element, 'menubar');
}
function isMenu(element) {
    return hasClass(element, 'menu');
}
function isPopupMenu(element) {
    return isMenu(element) && hasClass(element, 'popup')
}
function hasSubmenu(menuItem) {
    return !!menuItem.querySelector('.menu');
}

/* Menu item helpers */
function isMenuItemSelectable(menuItem) {
    return !(hasClass(menuItem, 'disabled') || hasClass(menuItem, 'separator'));
}
function isMenuItemSelected(menuItem) {
    return hasClass(menuItem, 'selected') || hasClass(menuItem, 'hover');
}
function deselectMenuItem(menuItem) {
    removeClass(menuItem, 'selected');
    removeClass(menuItem, 'hover');
    removeClassFromAllChildren(menuItem, 'selected');
    removeClassFromAllChildren(menuItem, 'hover');
}
function selectMenuItem(menuItem) {
    var menu = menuItem.parentElement;
    removeClassFromAllChildren(menu, 'selected');
    removeClassFromAllChildren(menu, 'hover');
    if(isMenubar(menu)) {
        addClass(menuItem, 'selected');
    } else {
        addClass(menuItem, 'hover');
    }
}
function triggerMenuItem(menuItem) {
    selectMenuItem(menuItem);
    if(hasSubmenu(menuItem)) {
        selectMenuItem(menuItem.querySelector('li:first-child'));
    } else {
        var parent = menuItem.parentElement;
        while(!isMenubar(parent) && !isPopupMenu(parent)) {
            parent = parent.parentElement;
        }
        removeClassFromAllChildren(parent, 'selected');
        removeClassFromAllChildren(parent, 'hover');
        if(isPopupMenu(parent)) {
            parent.remove();
        }

        menuItem.dispatchEvent(new Event('trigger'));
    }
}

/* Menu DOM traversal helpers */
function getMenuItem(element) {
    if(!element) return;
    if(element.tagName == 'U') {
        element = element.parentElement;
    }
    if(hasClass(element, 'label')) {
        return getMenuItem(element.parentElement);
    } else if(element.tagName == 'LI' && isMenu(element.parentElement)) {
        return element;
    }
}
function getMenu(element) {
    if(!element) return;
    if(isMenu(element)) {
        return element;
    } else {
        var menuItem = getMenuItem(element);
        if(menuItem && isMenu(menuItem.parentElement)) {
            return menuItem.parentElement;
        }
    }
}

/* Menu behavior */
window.addEventListener('click', function(event) {
    var menuItem = getMenuItem(event.target);
    var menu = getMenu(menuItem);
    if(menu && isMenubar(menu)) {
        if(hasClass(menuItem, 'selected')) {
            removeClass(menuItem, 'selected');
        } else {
            selectMenuItem(menuItem);
        }
        event.stopPropagation();
    } else if(menu) {
        if(!hasSubmenu(menuItem)) {
            triggerMenuItem(menuItem);
        }
        event.stopPropagation();
    } else {
        document.querySelectorAll('.menu .selected, .menu .hover')
                .forEach(function(menuItem) {
            deselectMenuItem(menuItem);
            event.stopPropagation();
        });
        document.querySelectorAll('.menu.popup')
                .forEach(function(menu) {
            menu.remove();
        });
    }
});
window.addEventListener('mouseover', function(event) {
    var menuItem = getMenuItem(event.target);
    var menu = getMenu(menuItem);
    if(menu) {
        var selected = menu.querySelectorAll('.selected, .hover');
        if(isMenubar(menu)) {
            if(selected.length > 0) {
                selected.forEach(function(menuItem) {
                    if(selected != menuItem) {
                        deselectMenuItem(menuItem);
                    }
                });
                addClass(menuItem, 'selected');
            }
        } else {
            if(isMenuItemSelectable(menuItem)) {
                selectMenuItem(menuItem);
            }
        }
    }
});
window.addEventListener('keydown', function(event) {
    var allSelected = document.querySelectorAll('.menubar .selected, .menubar .hover,' +
                                                '.menu.popup .selected, .menu.popup .hover');
    if(allSelected.length == 0) return;

    var selected = allSelected[allSelected.length - 1];
    var outSelected, newSelected;
    var isMenubarItem = isMenubar(getMenu(selected));

    if(isMenubarItem && event.key == 'ArrowRight' ||
            !isMenubarItem && event.key == 'ArrowDown') {
        outSelected = selected;
        newSelected = selected.nextElementSibling;
        while(newSelected && !isMenuItemSelectable(newSelected)) {
            newSelected = newSelected.nextElementSibling;
        }
        if(!newSelected) {
            newSelected = outSelected.parentElement.firstElementChild;
        }
    } else if(isMenubarItem && event.key == 'ArrowLeft' ||
                !isMenubarItem && event.key == 'ArrowUp') {
        outSelected = selected;
        newSelected = selected.previousElementSibling;
        while(newSelected && !isMenuItemSelectable(newSelected)) {
            newSelected = newSelected.previousElementSibling;
        }
        if(!newSelected) {
            newSelected = outSelected.parentElement.lastElementChild;
        }
    } else if(!isMenubarItem && event.key == 'ArrowRight') {
        if(hasSubmenu(selected)) {
            selectMenuItem(selected.querySelector('li:first-child'));
        } else {
            outSelected = allSelected[0];
            newSelected = outSelected.nextElementSibling;
            if(!newSelected) {
                newSelected = outSelected.parentElement.firstElementChild;
            }
        }
    } else if(!isMenubarItem && event.key == 'ArrowLeft') {
        if(allSelected.length > 2) {
            outSelected = selected;
        } else {
            outSelected = allSelected[0];
            newSelected = outSelected.previousElementSibling;
            if(!newSelected) {
                newSelected = outSelected.parentElement.lastElementChild;
            }
        }
    } else if(isMenubarItem && event.key == 'ArrowDown') {
        newSelected = selected.querySelector('li:first-child');
    } else if(event.key == 'Enter') {
        triggerMenuItem(selected);
    } else if(event.key == 'Escape') {
        outSelected = allSelected[0];
    } else {
        var withMnemonic = getMenu(selected).querySelector('.mnemonic-' + event.key);
        if(withMnemonic) {
            triggerMenuItem(withMnemonic);
        }
    }

    if(outSelected) deselectMenuItem(outSelected);
    if(newSelected) selectMenuItem(newSelected);

    event.stopPropagation();
});

/* Mnemonic behavior */
window.addEventListener('keydown', function(event) {
    var withMnemonic;
    if(event.altKey && event.key == 'Alt') {
        addClass(document.body, 'mnemonic');
    } else if(!isModal() && event.altKey && (withMnemonic =
                document.querySelector('.menubar > .mnemonic-' + event.code))) {
        triggerMenuItem(withMnemonic);
        event.stopPropagation();
    } else {
        removeClass(document.body, 'mnemonic');
    }
});
window.addEventListener('keyup', function(event) {
    if(event.key == 'Alt') {
       removeClass(document.body, 'mnemonic');
    }
});
