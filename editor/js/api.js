mergeInto(LibraryManager.library, {
    js_get_win_width: function () {
        return window.screen.width;
    },

    js_get_win_height: function () {
        return window.screen.height;
    },

    js_open_file_dialog: function () {
        return Asyncify.handleSleep(function (wakeUp) {
            var input = document.createElement('input')
            input.type = 'file'
            input.accept = 'application/json'

            input.onchange = e => {
                var file = e.target.files[0]
                var reader = new FileReader()

                console.log(file);

                reader.readAsText(file, 'UTF-8')

                reader.onload = readerEvent => {
                    var content = readerEvent.target.result; // file content

                    console.log(content);

                    var len = lengthBytesUTF8(content) + 1
                    var ptr = Module._malloc(len)

                    stringToUTF8(content, ptr, len)

                    wakeUp(ptr)
                } 
            }

            input.click()
        })
    },

    js_download: function(file_name, file_content) {
        console.log(file_content);

        var element = document.createElement('a')

        element.setAttribute('href', 'data:text/plain;charset=utf-8, ' + encodeURIComponent(UTF8ToString(file_content)))
        element.setAttribute('download', UTF8ToString(file_name))
        element.click()
    }
});