"""Korean punctuation emphasis without altering literal code or HTML safety."""
from learning_markdown import make_markdown
from markdown_it import MarkdownIt
md=make_markdown()
cases={
 '**핸들(handle)**은':'<strong>핸들(handle)</strong>은',
 '*강조(설명)*입니다':'<em>강조(설명)</em>입니다',
 '**`ptr`**를':'<strong><code>ptr</code></strong>를',
 '한**(강조)**글':'한<strong>(강조)</strong>글',
 '**앞말:**내용':'<strong>앞말:</strong>내용',
 '(**강조**)':'(<strong>강조</strong>)',
 '***강조(설명)***입니다':'<em><strong>강조(설명)</strong></em>입니다',
 '**바깥 *안쪽* (설명)**은':'<strong>바깥 <em>안쪽</em> (설명)</strong>은',
 '**[참고](https://example.com/)**를':'<strong><a href="https://example.com/">참고</a></strong>를',
 '`**핸들(handle)**은`':'<code>**핸들(handle)**은</code>',
 r'\*\*핸들(handle)\*\*은':'**핸들(handle)**은',
 '<script>alert(1)</script>':'&lt;script&gt;alert(1)&lt;/script&gt;',
 '[잘못된 링크](javascript:alert(1))':'[잘못된 링크](javascript:alert(1))',
 '**미완성(설명)':'**미완성(설명)',
}
for source,expected in cases.items():
 actual=md.renderInline(source);assert actual==expected,(source,actual,expected)
 assert '\u200b' not in actual and '\ufeff' not in actual
classic=MarkdownIt('commonmark',{'html':False}).enable('table').enable('strikethrough')
for source in ['a**(word)**b','a_b_c','_강조(설명)_입니다','* plain *','2 * 3 = 6','a***b**c*',r'\*literal\*','`char* p;`','`` **a** `ptr` ``','```cpp\nchar* p; // **(literal)**한\n```','    **(literal)**한','~~삭제~~','[**설명**](https://example.test/?a=1&b=2)']:
 assert md.render(source)==classic.render(source),source
print(f'Markdown: {len(cases)} Korean/literal/safety cases and 13 unchanged CommonMark cases passed')
