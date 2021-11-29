// https://stackoverflow.com/questions/22577457/update-data-on-a-page-without-refreshing
window.addEventListener('load', function()
{
    var xhr_title = null;
    var xhr_content = null;
    
    updateLiveData_Content = function()
    {
        var url = 'configcontent';
        xhr_content = new XMLHttpRequest();
        xhr_content.onreadystatechange = evenHandler_content;
        xhr_content.open("GET", url, true);
        xhr_content.send(null);
    };

    updateLiveData_Once = function()
    {
        xhr_title = new XMLHttpRequest();
        xhr_title.open("GET", "title", true);
        xhr_title.onreadystatechange = evenHandler_title;
        xhr_title.send(null);
    };

    updateLiveData_Once();
    updateLiveData_Content();

    function evenHandler_content()
    {
        if(xhr_content.readyState == 4 && xhr_content.status == 200)
        {
            dataDiv = document.getElementById('configuration');
            dataDiv.innerHTML = xhr_content.responseText;
        }
    }
    function evenHandler_title()
    {
        if(xhr_title.readyState == 4 && xhr_title.status == 200)
        {
            document.title = xhr_title.responseText;
            document.getElementById('title2').innerHTML = xhr_title.responseText;
        }
    }
});