
var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
connection.onopen = function () {
  connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
  console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
  console.log('Server: ', e.data);
};
connection.onclose = function () {
  console.log('WebSocket connection closed');
};

function sendeffect () {
var effect = document.getElementsByName("Effectfv1");
for(i = 0; i < effect.length; i++) {
                if(effect[i].checked)
               var effectstr = '#' + effect[i].value.toString(16);
                       
            }


  console.log('Effect: ' + effectstr);
  connection.send(effectstr);

}

