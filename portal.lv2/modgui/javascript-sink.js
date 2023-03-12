function (event) {
    function open_close_portal(isOpen) {
        var elem = event.icon.find('.falktx-portal-sink-light');
        if (isOpen) {
            elem.addClass('open');
        } else {
            elem.removeClass('open');
        }
    }

    if (event.type === 'start') {
        for (var i in event.ports) {
            if (event.ports[i].symbol === 'status') {
                open_close_portal(event.ports[i].value == 1);
                break;
            }
        }
    } else if (event.type === 'change') {
        if (event.symbol === 'status') {
            open_close_portal(event.value == 1);
        }
    }
}
