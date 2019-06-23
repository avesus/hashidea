google.charts.load('current', {'packages':['corechart']});
//google.charts.setOnLoadCallback(drawCharts);

let xvar = 'nthreads';
let xaxis = "discrete-float";
let yvar = "Milliseconds-Median";
let remainSet = {'trialsToRun': "5", 'alpha':"0.500000", 'beta':"0.500000", 'queryPercentage': "0.900000", 'HashAttempts': "2", 'cooloff': "2"};
let skipSet = {};
let skipBoxes = {};

function addListener(id, ename, cb) {
    document.getElementById(id).addEventListener(ename, cb);
}

function selectXaxis(evt) {
    xvar = evt.target.value;
    console.log(`variable to plot on X is ${xvar}`);
}

function selectXvar(evt) {
    xaxis = evt.target.value;
    console.log(`Method to plot X is ${xaxis}`);
}

function selectYvar(evt) {
    yvar = evt.target.value;
    console.log(`variable to plot on Y is ${yvar}`);
}

function selectRemain(evt) {
    console.log(evt.target);
    console.log(evt.target.id);
    console.log(evt.target.value);
    console.log(evt.target.nodeName);
    if (evt.target.nodeName == 'INPUT') {
        // this is a field select
        if (evt.target.checked == false) {
            delete remainSet[evt.target.value];
            let opt = document.querySelector(`select#holdgroup-${evt.target.value} option[value='novalue']`);
            opt.selected = true;
            let  skip = document.querySelector(`#skip input[value=${evt.target.value}]`);
            skip.disabled=false;
        } else {
            remainSet[evt.target.value] = 'novalue';
            let  skip = document.querySelector(`#skip input[value=${evt.target.value}]`);
            skip.checked = false;
            skip.disabled = true;
            removeFromSkipSet(evt.target.value);
        }
    } else {
        // this is a value of a field select
        let field = evt.target.id.substring(10);
        remainSet[field] = evt.target.value;
    }
}

let cbid = 1;

function removeFromSkipSet(field) {
    if (field in skipSet) {
        delete skipSet[field];
        // now delete all value checkboxes
        for (let divid of skipBoxes[field]) {
            let div = document.getElementById(divid);
            div.parentNode.removeChild(div);
        }
        delete skipBoxes[field];
    }
}

function selectSkip(evt) {
    let field = evt.target.value;
    if (field.indexOf("-skip-") > 0) {
        // this is value to be skipped
        console.log(field);
        let [ parent, value ] = field.split('-skip-');
        skipSet[parent][value] = evt.target.checked; 
    } else {
        // this is a field name
        if (evt.target.checked) {
            // add this to set
            skipSet[field] = {};
            skipBoxes[field] = [];
            // now add all checkboxes
            let fielddiv = evt.target.parentNode;
            for (let value in window.selector[field]) {
                let [id, box] = makeCheckBox(`${field}-skip-${value}`, value, false);
                box.className = 'valgroup';
                box.id = `chkbox${cbid}`;
                cbid++;
                fielddiv.appendChild(box);
                addListener(id, 'change', selectSkip);
                skipSet[field][value] = false;
                skipBoxes[field].push(box.id);
            }
        } else if (field in skipSet) {
            // delete from set
            removeFromSkipSet(field);
        }
    }
};

function makeOption(value, labtext, checked) {
    let option = document.createElement("option");
    console.log(`${value} => ${labtext}`);
    option.text = value;
    option.value = labtext;
    if (checked) option.selected = true;
    return option;
}

function addSelections(selectId, values, onSelectFunction, initial) {
    let node = document.getElementById(selectId);
    for (let value of values) {
        node.appendChild(makeOption(value, value, (initial == value)));
    }
    addListener(selectId, 'change', onSelectFunction);
}

function makeCheckBox(value, labeltext, checked) {
    console.log(`makeCheckBox(${value}, ${labeltext}, ${checked})`);
    let box = document.createElement("input");
    box.type = "checkbox";
    box.id = `chkbox${cbid}`;
    cbid++;
    box.value = value;
    if (checked) box.checked = true;

    let label = document.createElement("label");
    label.for = box.id;
    label.append(labeltext);

    let div = document.createElement("div");
    div.appendChild(box);
    div.appendChild(label);

    return [box.id, div];
}

function makeRadioButton(groupname, value, labeltext, checked) {
    let box = document.createElement("input");
    box.type = "radio";
    box.name = groupname;
    box.id = `chkbox${cbid}`;
    cbid++;
    box.value = value;
    if (checked) box.checked = true;

    let label = document.createElement("label");
    label.for = box.id;
    label.append(labeltext);

    let div = document.createElement("div");
    div.appendChild(box);
    div.appendChild(label);

    return [box.id, div];
}


function addCheckboxes(parent, values, onCheckFunction, initialMap) {
    console.log(initialMap);
    let node = document.getElementById(parent);
    for (let value of values) {
        // select the variable
        console.log(value);
        let [boxid, div] = makeCheckBox(value, value, (value in initialMap));
        node.appendChild(div);
        addListener(boxid, 'change', onCheckFunction);

        // select the value if we select the variable
        let valdiv = document.createElement("select");
        valdiv.className = "valgroup";
        valdiv.id = `holdgroup-${value}`;
        div.appendChild(valdiv);
        valdiv.appendChild(makeOption("-- select a value --", "novalue", false));
        for (let data in window.selector[value]) {
            valdiv.appendChild(makeOption(`${data} (${window.selector[value][data]})`, data, (value in initialMap) && (data == initialMap[value])));
            addListener(valdiv.id, 'change', onCheckFunction);
        }
    }
}

function addSkipboxes(parent, values, onCheckFunction) {
    let node = document.getElementById(parent);
    for (let value of values) {
        // select the variable
        let [boxid, div] = makeCheckBox(value, value, false);
        node.appendChild(div);
        if (value in remainSet) document.getElementById(boxid).disabled = true;
        addListener(boxid, 'change', onCheckFunction);
    }
}

function getval(row, field) {
    let col = window.selector['name2index'][field];
    if (field in window.selector['name2number']) {
        return parseFloat(row[col]);
    }
    return row[col];
}

function makecolumns(left, tree, prefix, out) {
    if (left == 0) {
        out.push(prefix);
        return;
    }
    var level = [];
    for (let key in tree) {
        level.push(key);
    }
    level.sort();
    for (let field of level) {
        makecolumns(left-1, tree[field], [...prefix, field], out);
    }
}

// using selection above, draw the graph
function redraw() {
    let selectedRows = [];
    for (let row of window.data) {
        // check that all remain data is valid
        let keep = true;
        for (let remain in remainSet) {
            if (row[window.selector['name2index'][remain]] != remainSet[remain]) {
                keep = false;
                break;
            }
        }
        if (!keep) continue;
        // check that skip values are not in row
        for (let skip in skipSet) {
            if (row[window.selector['name2index'][skip]] == skipSet[skip]) {
                keep = false;
                break;
            }
        }
        if (!keep) continue;
        // we want this row
        selectedRows.push(row);
    }
    // calculate columns which are varying
    let varies = [];
    for (let colname of window.selector['controllers']) {
        if (colname in remainSet) continue;
        if (colname == xvar) continue;
        varies.push(window.selector['name2index'][colname]);
        console.log(`${colname} is vary: ${window.selector['name2index'][colname]}`);
    }
    console.log(varies);
    // calculate min/max x values and different trendlines
    let trendtree = {};
    let xmin = getval(selectedRows[0], xvar);
    let xmax = xmin;
    let allxs = {};
    for (let row of selectedRows) {
        let tree = trendtree;
        for (let vidx of varies) {
            if (!(row[vidx] in tree)) {
                tree[row[vidx]] = {};
            }
            tree = tree[row[vidx]];
        }
        let val = getval(row, xvar);
        if (val < xmin) xmin=val;
        if (val > xmax) xmax=val;
        allxs[val] = 1;
        tree[val] = getval(row, yvar);
    }
    console.log(`${xvar} from ${xmin} to ${xmax} in ${selectedRows.length}`);
    console.log(trendtree);
    
    // now put the data in proper form
    let headers = [ ];
    makecolumns(varies.length, trendtree, [], headers);
    console.log(headers);

    let trendnames = headers.map(x => x.join("-"));
    let gdata = [ [ xvar, ...trendnames ]  ];
    console.log(gdata);
    let xrange = [];

    for (let x in allxs) {
        let xv = x;
        if (xvar in window.selector['name2number']) {
            xv = parseFloat(x);
            console.log(`converting ${x} to ${xv}`);
        }
        xrange.push(xv);
    }
    if ((xvar in window.selector['name2number']))
        xrange.sort((a, b) => a - b);
    else
        xrange.sort();
        
    for (let x of xrange) {
        console.log(`============> ${x}`);
        let rowdata = [ x ];
        for (let col of headers) {
            let tree = trendtree;
            for (let field of col) {
                tree = tree[field];
            }
            rowdata.push(tree[x]);
        }
        gdata.push(rowdata);
        console.log(rowdata);
    }
    console.log(gdata);
    var tabledata = google.visualization.arrayToDataTable(gdata, false);

    let chart = new google.visualization.LineChart(document.getElementById('graphdiv'));
    

    // options for this graph
    const options = {
	hAxis: {
	    title: xvar,
	    format: '####',
	    minValue: xmin,
	    maxValue: xmax,
        },
        vAxis: {
	    title: yvar,
        },
        chart: {
            title: `Performance as we vary ${xvar}`,
        },
	pointSize: 5,
	interpolateNulls: true,
        width: 1200,
        height: 500,
	explorer: { 
            actions: ['dragToZoom', 'rightClickToReset'],
            //axis: 'horizontal',
            keepInBounds: true,
            maxZoomIn: 4.0},	  
    };
    
    chart.draw(tabledata, options);    
}

// build all buttons for creating graphs
function buildButtons() {
    // X
    addListener('xaxis', 'change', selectXaxis);
    addSelections('xvar', window.selector['controllers'], selectXvar, xvar);
    // Y
    addSelections('yvar', window.selector['index2name'], selectYvar, yvar);
    // Lines
    addCheckboxes('remain', window.selector['controllers'], selectRemain, remainSet);
    addSkipboxes('skip', window.selector['controllers'], selectSkip);
    // Button
    addListener('drawgraph', 'click', redraw);
}

document.addEventListener("DOMContentLoaded", buildButtons);


