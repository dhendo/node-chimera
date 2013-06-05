var Chimera = require(__dirname + '/../lib/main').Chimera;

var c = new Chimera();
var start = new Date();
c.perform({
    url: "http://www.google.com",
    callback: function (err, result) {
        console.log('capture screen shot to a buffer');
        var imageBuffer = c.browser.captureBytes();
        var fs = require('fs');
        fs.writeFileSync("direct.png", imageBuffer);
        // We now have a buffer containing the PNG image data. we can send this directly over the wire without touching the filesystem.

        // Or write it to the filesystem for demo purposes. Whatever.
        console.log("All done. Took " + (new Date() - start) + 'ms');

        c.close();
        process.exit();
    }
});
