var Chimera = require(__dirname + '/../lib/main').Chimera;

var c = new Chimera();
var start = new Date();
var htmlSnippet =
    '<html>' +
    '<head></head>' +
    '<body>' +
    '<div id="theDiv" style="height: 100px; width: 500px; border: 4px solid #f00; background-size: 50px 50px; background-color: #fc0; background-image: -webkit-linear-gradient(-45deg, rgba(255, 255, 255, .2) 25%, transparent 25%, transparent 50%, rgba(255, 255, 255, .2) 50%, rgba(255, 255, 255, .2) 75%, transparent 75%, transparent);text-align: center; color:#bbb; text-shadow: #888 1px 1px;"><h1 style="line-height: 100px; margin: 0px;">Hello World</h1></div>' +
    '</body>' +
    '</html>';

c.perform({
    html: htmlSnippet,

    callback: function (err, result) {
        // Clip the capture to our Div
        c.browser.clipToElement("#theDiv");
        // Capture to File
        c.browser.capture('html-render.png');

        console.log("All done. Took " + (new Date() - start) + 'ms');
        c.close();
        process.exit();
    }
});
