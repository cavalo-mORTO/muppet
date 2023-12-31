var videoPlayer = function () {
    var title = document.getElementById('video-title');
    var playpause = document.getElementById('playpause');
    var ffplus = document.getElementById('ffplus');
    var fplus = document.getElementById('fplus');
    var ffminus = document.getElementById('ffminus');
    var fminus = document.getElementById('fminus');
    var mute = document.getElementById('mute');
    var progress = document.getElementById('progress');
    var progressBar = document.getElementById('progress-bar');

    // Changes the button state of certain button's so the correct visuals can be displayed with CSS
    var changeButtonState = function(type) {
        // Play/Pause button
        if (type == 'playpause') {
		playpause.setAttribute('data-state', video.pause ? 'pause' : 'play');
		document.getElementById("playpause_iframe").setAttribute('class', video.pause ? "fa fa-play" : 'fa fa-pause');

        }
        // Mute button
        else if (type == 'mute') {
		mute.setAttribute('data-state', video.mute ? 'mute' : 'unmute');
		document.getElementById("mute_iframe").setAttribute('class', video.mute ? "fa fa-volume-up" : 'fa fa-volume-mute');
        }
    }

    var init = function () {
        // Obtain handles to main elements
        var videoControls = document.getElementById('video-controls');

        // Display the user defined video controls
        videoControls.setAttribute('data-state', 'visible');

        // Obtain handles to buttons and other elements
        var stop = document.getElementById('stop');
        var volinc = document.getElementById('volinc');
        var voldec = document.getElementById('voldec');
        var sub = document.getElementById('sub');
        var audio_track = document.getElementById('audio_track');
        var video_track = document.getElementById('video_track');

        // If the browser doesn't support the progress element, set its state for some different styling
        var supportsProgress = (document.createElement('progress').max !== undefined);
        if (!supportsProgress) progress.setAttribute('data-state', 'fake');

        // Check the volume
        var checkVolume = function(dir) {
            if (dir) {
                if (dir === '+') {
                    var vol = video.volume + 5;
                    if (vol <= 100)
                        ws.send('{ "command": ["set_property", "volume", ' + vol + '] }\n');
                }
                else if (dir === '-') {
                    var vol = video.volume - 5;
                    if (vol >= 0)
                        ws.send('{ "command": ["set_property", "volume", ' + vol + '] }\n');
                }

                ws.send('{ "command": ["set_property", "mute", ' + (video.volume <= 0) + '] }\n');
            }
            changeButtonState('mute');
        }

        // Change the volume
        var alterVolume = function(dir) {
            checkVolume(dir);
        }

        // Only add the events if addEventListener is supported (IE8 and less don't support it, but that will use Flash anyway)
        if (document.addEventListener) {
            title.addEventListener('paste', function(e) {
                var clip = e.clipboardData.getData('text/plain');
                ws.send('{ "command": ["loadfile", "' + clip + '"] }\n');
            });

            // Add events for all buttons			
            playpause.addEventListener('click', function(e) {
                ws.send('{ "command": ["cycle", "pause"] }\n');
            });

		ffplus.addEventListener('click', function(e) {
			var inc = video["playback-time"] + 300;
			ws.send('{ "command": ["set_property", "playback-time",' + inc + '] }\n');
		});

		fplus.addEventListener('click', function(e) {
			var inc = video["playback-time"] + 30;
			ws.send('{ "command": ["set_property", "playback-time",' + inc + '] }\n');
		});

		ffminus.addEventListener('click', function(e) {
			var dec = video["playback-time"] - 300;
			ws.send('{ "command": ["set_property", "playback-time",' + dec + '] }\n');
		});

		fminus.addEventListener('click', function(e) {
			var dec = video["playback-time"] - 30;
			ws.send('{ "command": ["set_property", "playback-time",' + dec + '] }\n');
		});

            // The Media API has no 'stop()' function, so pause the video and reset its time and the progress bar
            stop.addEventListener('click', function(e) {
                ws.send('{ "command": ["set_property", "pause", true] }\n');
                ws.send('{ "command": ["set_property", "playback-time", 0] }\n');
                progress.value = 0;
                // Update the play/pause button's 'data-state' which allows the correct button image to be set via CSS
                changeButtonState('playpause');
            });
            mute.addEventListener('click', function(e) {
                ws.send('{ "command": ["cycle", "mute"] }\n');
                changeButtonState('mute');
            });
            volinc.addEventListener('click', function(e) {
                alterVolume('+');
            });
            voldec.addEventListener('click', function(e) {
                alterVolume('-');
            });
            sub.addEventListener('click', function(e) {
                ws.send('{ "command": ["cycle", "sub"] }\n');
            });
            audio_track.addEventListener('click', function(e) {
                ws.send('{ "command": ["cycle", "audio"] }\n');
            });
            video_track.addEventListener('click', function(e) {
                ws.send('{ "command": ["cycle", "video"] }\n');
            });

            // React to the user clicking within the progress bar
            progress.addEventListener('click', function(e) {
                var pos = (e.pageX  - (this.offsetLeft + this.offsetParent.offsetLeft)) / this.offsetWidth;
                pos *= 100;
                ws.send('{ "command": ["seek",' + pos + ', "absolute-percent"] }\n');
            });
        }
    }

    var update = function () {
        if ("media-title" in video)
            title.innerHTML = video["media-title"];

        if ("playback-time" in video && "duration" in video) {
            progress.max = video["duration"];
            progress.value = video["playback-time"];

            progressBar.style.width = Math.floor((video["playback-time"] / video.duration) * 100) + '%';
        }

        if ("pause" in video)
            changeButtonState("playpause");

        if ("mute" in video)
            changeButtonState("mute");
    }

    return {
        init: init,
        update: update
    };
};
