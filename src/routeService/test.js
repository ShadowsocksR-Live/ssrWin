var net = require('net');
var domain = require('domain');

var remoteProxyIp = process.argv[2];
if (remoteProxyIp == undefined) {
    remoteProxyIp = "123.45.67.89";
}

var addr = '\\\\?\\pipe\\ssrRouteServicePipe';

var d = domain.create();
d.on('error', function(err) {
    console.error(err);
});

d.run(function() {
    var client = net.createConnection({ path: addr }, function() {
        console.log("Connected");
        client.on('data', function(data) {
            console.log("Recieved: " + data);
        });
        client.on('error', function(){
            console.log(arguments);
        });

        client.on('end', function() { 
            console.log('Disconnecting server...');
        });

        const cleanup = () => {
            client.removeAllListeners();
            process.exit();
        };
        client.once('close', cleanup);
        client.once('error', cleanup);

        client.write(`{"action":"configureRouting","parameters":{"proxyIp":"${remoteProxyIp}","isAutoConnect":false}}`);

    }.bind(this));

    function timeoutQuit(arg) {
        // client.destroy();
        process.exit();
    }

    process.on('SIGINT', function() {
        console.log("Caught interrupt signal");
        client.write('{"action":"resetRouting","parameters":{}}');
        setTimeout(timeoutQuit, 2000, 'JustExit');
    });
});
