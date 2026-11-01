from pathlib import Path
root=Path(__file__).resolve().parent.parent
source=(root/'B25DepthHelper/B140aDirectPresentSharedOpen.cpp').read_text()
function=source[source.index('static bool take_latest_completed_job('):source.index('static bool b173h_take_live_job(')]
target=root/'tests/PostSleepRefresh.generated.cpp'
text=target.read_text()
prefix=text[:text.index('static bool take_latest_completed_job(')]
body=text[text.index('int main(){'):]
target.write_text(prefix+function+body)
print('Generated production scheduler refresh regression')
