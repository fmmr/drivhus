  // variables for the first series
  var series_1_channel_id = 2568299;
  var series_1_field_number = 1;
  var series_1_read_api_key = '';
  var series_1_results = 30;
  var series_1_color = '#d62020';

  // variables for the second series
  var series_2_channel_id = 2584548;
  var series_2_field_number = 1;
  var series_2_read_api_key = '';
  var series_2_results = 30;
  var series_2_color = '#00aaff';

  // variables for the second series
  var series_3_channel_id = 2584548;
  var series_3_field_number = 2;
  var series_3_read_api_key = '';
  var series_3_results = 30;
  var series_3_color = '#0000ff';

  // variables for the second series
  var series_4_channel_id = 2584548;
  var series_4_field_number = 4;
  var series_4_read_api_key = '';
  var series_4_results = 30;
  var series_4_color = '#ffaaff';

  // chart title
  var chart_title = 'Light & Temperature';
  // y axis title
  var y_axis_title = 'Values';

  // user's timezone offset
  var my_offset = new Date().getTimezoneOffset();
  // chart variable
  var my_chart;
  console.log("heisan");
  // when the document is ready
//  $(document).on('ready', function() {
	  console.log("hoppsan");
  	// add a blank chart
  	addChart();
  	// add the first series
  	addSeries(series_1_channel_id, series_1_field_number, series_1_read_api_key, series_1_results, series_1_color);
  	// add the second series
  	addSeries(series_2_channel_id, series_2_field_number, series_2_read_api_key, series_2_results, series_2_color);
  	addSeries(series_3_channel_id, series_3_field_number, series_3_read_api_key, series_3_results, series_3_color);
  	addSeries(series_4_channel_id, series_4_field_number, series_4_read_api_key, series_4_results, series_4_color);
  //});

  // add the base chart
  function addChart() {
  	// variable for the local date in milliseconds
  	var localDate;

  	// specify the chart options
  	var chartOptions = {
  		chart: {
  			renderTo: 'chart-container',
  			defaultSeriesType: 'line',
  			backgroundColor: '#ffffff',
  			events: {}
  		},
  		title: {
  			text: chart_title
  		},
  		plotOptions: {
  			series: {
  				marker: {
  					radius: 2
  				},
  				animation: true,
  				step: false,
  				borderWidth: 0,
  				turboThreshold: 0
  			}
  		},
  		tooltip: {
  			// reformat the tooltips so that local times are displayed
  			formatter: function() {
  				var d = new Date(this.x + (my_offset * 60000));
  				var n = (this.point.name === undefined) ? '' : '<br>' + this.point.name;
  				return this.series.name + ':<b>' + this.y + '</b>' + n + '<br>' + d.toDateString() + '<br>' + d.toTimeString().replace(/\(.*\)/, "");
  			}
  		},
  		xAxis: {
  			type: 'datetime',
  		},
  		yAxis: {
  			title: {
  				text: y_axis_title
  			}
  		},
  		exporting: {
  			enabled: false
  		},
  		legend: {
  			enabled: true
  		},
  		credits: {
  			text: 'drimon.rodland.no',
  			href: 'https://drimon.rodland.no/',
  			style: {
  				color: '#339933'
  			}
  		}
  	};

  	// draw the chart
  	my_chart = new Highcharts.Chart(chartOptions);
  }

  // add a series to the chart
  function addSeries(channel_id, field_number, api_key, results, color) {
  	var field_name = 'field' + field_number;

  	// get the data with a webservice call
  	$.getJSON('https://api.thingspeak.com/channels/' + channel_id + '/fields/' + field_number + '.json?offset=0&round=2&results=' + results + '&api_key=' + api_key, function(data) {

  		// blank array for holding chart data
  		var chart_data = [];

  		// iterate through each feed
  		$.each(data.feeds, function() {
  			var point = new Highcharts.Point();
  			// set the proper values
  			var value = this[field_name];
  			point.x = getChartDate(this.created_at);
  			point.y = parseFloat(value);
  			// add location if possible
  			if (this.location) {
  				point.name = this.location;
  			}
  			// if a numerical value exists add it
  			if (!isNaN(parseInt(value))) {
  				chart_data.push(point);
  			}
  		});

  		// add the chart data
  		my_chart.addSeries({
  			data: chart_data,
  			name: data.channel[field_name],
  			color: color
  		});
  	});
  }

  // converts date format from JSON
  function getChartDate(d) {
  	// offset in minutes is converted to milliseconds and subtracted so that chart's x-axis is correct
  	return Date.parse(d) - (my_offset * 60000);
  }
