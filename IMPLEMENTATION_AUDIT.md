# Kontroll mot de ursprungliga förslagen

Uppdaterat 2026-09-17 efter kompletteringen av klickpositioner, textformatering,
paket/makron och automatisk parning. Tabellen beskriver implementerade beteenden
och de praktiska gränserna för en lokal förhandsvisning.

| Ursprungligt förslag | Status | Implementation och kontroll |
| --- | --- | --- |
| En sammanhängande skrivyta | Klart | En gemensam markör och markering över hela dokumentet. Tangentbords- och mustest i `document_editor_regression.cpp`. |
| Bara aktuell formel visar kod | Klart | Två formler i samma mening renderas var för sig. Testet öppnar en och verifierar att den andra förblir renderad. |
| Text och matematik på samma sida | Klart | Text plus flera `$…$`-formler testas utan ändring av källtexten, även genom sparning och PDF-export. |
| Enter delar vid markören | Klart | Testar delning mitt i text. Inuti matematik bevaras miljön. |
| Backspace förenar stycken | Klart | Testar Backspace i början av ett stycke, med bibehållen markörposition. |
| Markera, kopiera och radera över styckegränser | Klart | Testar markering, verkligt urklippsinnehåll, radering och återställning. |
| Pilar behåller ungefärlig kolumn | Klart | Testar lodrät navigering mellan stycken. |
| Ångra/återställ bevarar skrivpositionen | Klart | Testar markör, markering och fokus efter undo/redo. |
| Klick placerar markören där man klickar | Klart för källmappade uttryck | SVG-grupper kopplar TeX-atomer till källan. Testar separata klick i täljare, nämnare och omgivande formeltecken. Makroexpansioner pekar på anropet; ovanliga konstruktioner som inte accepterar källmarkeringar använder ungefärlig placering. |
| Vanliga matematikavgränsare | Klart | `$…$`, `\(…\)`, `\[…\]` och `$$…$$` stöds. Inline- och displayrendering samt Shift+Enter med avgränsare kontrolleras. |
| Klistra in flerradig matematik | Klart | Ett inklistrat `align`-block förblir sammanhängande. Normal Enter och Shift+Enter testas separat. |
| Fristående matematik även inne i en källrad | Klart efter komplettering | `\[…\]` och `equation` får egna visuella rader mellan omgivande text. Testet kontrollerar höjdskillnaden och oförändrad källa. |
| Vanlig LaTeX-struktur och syntax | Kompletterat | Rubriker och dokumenterade matematikmiljöer samt nästlad `\textbf`, `\emph`, `\textit`, `\texttt` och relaterad textformatering stöds. Paket, egna makron och valfri dokumentklass överst i dokumentet används av preview och export. Testar `bm`, en matematikmakro, en textmakro med argument, ändrad definition, sparning och PDF. Paket måste finnas lokalt och fungera med LaTeX/DVI. Sidbrytningar och dokumentövergripande referenser verifieras i PDF. |
| Bevara äldre anteckningar | Klart inom verifierade fall | Äldre Math/Text-lägen migreras vid inläsning; fristående matematik får explicita LaTeX-avgränsare. Testar äldre format, flerradiga block, återställning och en äldre matrisformel med efterföljande matematik. |
| Automatisk rendering efter skrivpaus | Klart | Renderingskö med fördröjning, cache och en aktiv TeX-process. Tester inväntar automatisk rendering utan manuell åtgärd. |
| Stabil skrivposition under rendering | Klart inom verifierat fall | Testar att en renderad formel ovanför ett skrollat skrivläge inte flyttar markören på skärmen. Växling mellan källa och formel kan fortfarande ändra dokumentets höjd. |
| Ofärdig matematik är normalt under skrivning | Klart efter komplettering | Saknade avslut på avgränsare, grupper och miljöer lämnas som redigerbar källa och skickas inte för rendering. Detta är en avgränsarkontroll, inte en fullständig TeX-parser. |
| Diskreta fel efter paus | Implementerat, delvis testat | Renderingsfel visas med lokal markering och en hint efter skrivpaus. Backendens feldiagnostik testas; alla visuella feltillstånd har inte automatiserade tester. |
| Kommandoförslag vid markören | Klart | `\fra` + Tab infogar en bråkmall. Förslag finns för kommandokatalogen, inte varje möjligt TeX-kommando. |
| Tab mellan mallens argument | Klart | Testar Tab framåt och Shift+Tab bakåt. Mallar infogas som vanlig LaTeX; matematik i löptext får avgränsare. |
| Matchande klamrar | Klart | Testar att vanlig inmatning av `\frac{a}{b}` inte dubblerar automatiska avslut. |
| Hjälp med `\begin` och `\end` | Klart efter komplettering | Ett matchande avslut infogas när öppningstaggen skrivs färdigt. Testar befintliga avslut, nästling och kommentarer. Senare namnändringar synkroniseras inte automatiskt. |
| Lugnare dokumentlayout och färre synliga verktyg | Klart | Sammanhängande typografi och en verktygsmeny. Skärmbilder kontrollerade för text, matematik, rubrik och sats. |
| Ctrl+N börjar skriva direkt; metadata senare | Klart | Testar tomt dokument, fokus, bibehållen kurs, stängd inställningsdialog och omedelbar textinmatning. |
| Samverkan med den andra taskens Shift+Enter | Klart | Den implementationen har bevarats och integrerats med aktiv inlineformel. Separat regressionstest kontrollerar text, matematik, undo/redo, navigering och numeriskt Enter. |

## Verifiering

- `document_editor_regression`: riktiga Qt-tangent- och mushändelser, TeX-rendering,
  PDF-export och sparning/öppning.
- `block_break_regression`: den samordnade Shift+Enter-funktionen.
- `backend_regression`: bland annat äldre format, sparning, återställning,
  flerradiga block och export.
- `lecture_stress`: 1 500 rader och tidsgränser för skrivning, navigering,
  lägesbyten, sökning och renderingskö.
- Appbygge och `git diff --check`.

Editortestet jämför dessutom sex typer av källmappade formelbilder pixel för pixel
med vanlig TeX-rendering, så att klickstödet inte ändrar symboler eller avstånd.

Den äldre `keyboard_regression.cpp` för de borttagna radwidgetarna är inte
fullständigt migrerad. Det nya editortestet ersätter dess kontroller av
skrivytan; äldre dialogkontroller återstår att flytta. Godkända tester innebär
inte fullständig verifiering av alla gränssnittslägen eller all LaTeX-syntax.


## Tillagd skrivhjälp

- `$` ger `$|$`; ett till direkt ger `$$|$$`.
- `\(` och `\[` skapar respektive avslut med markören inuti.
- Ett skrivet avslut passerar det befintliga.
- Backspace mellan tomma avgränsare tar bort hela paret.
- Dollar, klamrar och parenteser omsluter markerad text; undo återställer markeringen.
- `\$` och dollar i kommentarer paras inte. AltGr-inmatning stöds.
- Textformatering finns också som kommandoförslag och Tab-mallar.
- Sista Tab lämnar hela mallen, inklusive avslutande klamrar och matematikavgränsare.
- Ofullständiga makrodefinitioner pausar preview tills definitionen har ett avslut.

`|` ovan visar markören. Källmappningen använder
[dvisvgms SVG-specials](https://dvisvgm.de/Manpage/).

Slutkontrollerna passerade: editorregression i ett vanligt X11-fönster,
backendregression, separat Shift+Enter-regression, appbygge och diffkontroll.
Det senaste godkända långdokumenttestet laddade 1 500 rader på 230 ms och skrev
30 tecken på 238 ms. 60 lägesbyten tog 2 480 ms mot gränsen 2 500 ms, vilket
fortfarande ger liten marginal för just det testet på denna dator.

## Lokal stavningskontroll (2026-09-17)

- Röd våglinje under felstavade ord; vänster- eller högerklick visar upp till sex förslag.
- Svenska och engelska väljs under ⋯ → Stavningskontroll. Svenska är standard i
  nya dokument och i äldre dokument som saknar språkval.
- Språk, på/av och ignorerade ord sparas med dokumentet. Ignorerade ord kan återställas.
- Rättning ersätter bara ordet, bevarar LaTeX-källan och kan ångras.
- Vanlig text, rubriker och bildtexter kontrolleras. Formler, kommandon med tekniska
  argument, preamble, kommentarer, URL:er och e-postadresser undantas. Text inne i
  formler kontrolleras inte. Detta är stavningskontroll, inte grammatikkontroll.
- Hunspell och medföljande sv_SE/en_US-ordlistor körs lokalt på en bakgrundstråd,
  efter 450 ms skrivpaus. Inga anteckningar skickas över nätet. Inaktuella resultat
  och rättningsförslag avvisas när källan eller språket ändras.

Verifierat med `tests/spelling_tests.pro`: faktiska ord- och förslagsklick i ett
X11-fönster, korrekt svenska med å/ä/ö, engelska, LaTeX-undantag, rättning/undo,
lagring av språk/ignorerade ord, svenska för gamla/nya dokument, på/av, inaktuella
förslag och dragmarkering. Våglinje och meny har inspekterats visuellt.
En bakgrundskontroll av 1 500 stycken tog 52 ms och UI-timern fortsatte ticka.

Appbygge, editorregression, backendregression och Shift+Enter-regression passerade.
Långdokumenttestet passerade efter att en redundant editorsynk vid lägesbyte togs
bort: 1 500 rader laddades på 217 ms, 30 tecken skrevs på 397 ms och 60 lägesbyten
på 2 359 ms. De befintliga Qt-varningarna om flera systemgenvägar kvarstår;
ingen ny QML-varning från stavningskontrollen observerades.

## Borttagna radlägen (2026-09-17)

- Text/Math/Auto-menyn, knapparna i typdialogen och Ctrl+Alt+M är borttagna.
- Nya stycken följer LaTeX-avgränsare i stället för att gissa utifrån exempelvis
  ett likhetstecken. Typmetadata för rubriker/listor/satser är separat och kvar.
- Vid öppning/återställning migreras äldre radlägen: blandad text med explicita
  formelavgränsare behåller exakt samma källa; fristående äldre matematik får
  `\[ … \]`. Preamble och bildtexter undantas. Migreringen är idempotent.
- Regressionen reproducerar stycket ”Om vi sätter in…” med tre formler, från
  samtliga äldre radlägen, och verifierar preview, PDF samt sparning/återöppning.
- Shift+Enter bevarar blanktecken runt formelavgränsare utanför nya aligned-block,
  så att migrerade formler inte får otillåtna tomma TeX-stycken.
- Editor-, backend-, stavnings- och Shift+Enter-tester passerade. Långdokumenttestet
  passerade med 1 500 rader (inläsning 210 ms, 30 skrivna tecken 325 ms).
  Mätningen för det borttagna radlägeskommandot togs bort ur stresstestet.

## Pilnavigering och centrerad displaymatematik (2026-09-17)

- Upp/Ned följer verkliga visuella rader i stället för ett fast pixelavstånd.
  Höga formler och styckeavstånd kan därför inte få markören att fastna på samma rad.
- Navigering in i en renderad formel använder formelns källmappning, även när
  textkolumnen ligger utanför den centrerade formelbilden. Shift-markering bevaras.
- Displaymatematik får ett centrerat visuellt stycke; omgivande text återfår
  normal justering. Källtexten ändras inte. Befintliga radbrytningar återanvänds.
- Regressioner för uppåt/nedåt från båda sidor, Shift+Upp, höga bråk, align,
  display mitt i ett textstycke, centrering och fönsterstorlek passerade, liksom
  befintliga källmappade musklick, Shift+Enter och stavningskontroll.
- Centreringen inspekterades visuellt. Appbygge och långdokumenttest passerade:
  1 500 rader laddades på 243 ms och 30 tecken skrevs på 375 ms.

## Kontextmedvetna dollartecken (2026-09-17)

- Ett extra `$` före/efter ett befintligt öppningstecken eller efter motsvarande
  sluttecken uppgraderar `$…$` till `$$…$$` i en enda ångringsbar ändring.
- Befintliga avslut passeras; dubbelavgränsare får inte ett tredje tecken.
- Inne i matematik och vid avslut av ofärdiga uttryck infogas bara ett `$`.
- Backspace/Delete i en dubbelavgränsare reducerar båda ändarna till enkla dollar.
  Tomma automatiska par kan fortfarande raderas tillsammans.
- En nyligen skapad tom inline-parning spåras separat, så att den inte förväxlas
  med de två öppningstecknen i en redan ifylld displayformel.
- Native editortester passerade för gränspositioner, ofärdiga uttryck, ångra/gör om,
  flera/angränsande formler, kommentarer, escaped dollar, AltGr och befintliga
  redigerings-, navigerings-, centrerings- och exportflöden.

## Stabil vy vid Tab och källändringar (2026-09-17)

- Inspelningens hopp reproducerades i en nedscrollad anteckning: Tab-expansion
  av `$frac$` flyttade markören från y=220 till y=251,75 och scrollpositionen
  från 1082 till 1050,25 trots att texten låg kvar på samma rad.
- Mallar ersätter nu triggern och markerar första fältet i en atomisk ändring,
  utan mellanlägen med borttagen trigger eller markören i slutet av expansionen.
- Redigering förankrar vyn vid en källposition som mättes före ändringen. Nya
  källoffset används inte längre för att mäta den gamla layouten.
- Vid återställning av dokumentrader mappas markören per rad och kolumn, så att
  kortare källtext inte tillfälligt placerar markören i nästa stycke.
- Native regressioner verifierar scroll/markörposition vid expansion, nästa/
  föregående mallfält, undo/redo och inmatning av LaTeX-kommandon. Testerna för
  befintlig pilnavigering, centrering, smarta dollar och export passerade också.
- Slutkontroller: appbygge, Shift+Enter, stavningskontroll och långdokumenttest
  passerade. Inläsning av 1 500 rader tog 301 ms och 30 skrivna tecken 301 ms.

## Explicita radbrytningar i löptext (2026-09-18)

- `\\` i löptext visas som radbrytning och kan redigeras genom att flytta
  markören till kommandot eller använda källvyn. Kopiering behåller källan exakt.
- En efterföljande källnyrad räknas inte som ytterligare en visuell brytning;
  radslut använder den befintliga styckegränsen utan extra tomrad.
- TeX/PDF-export bevarar explicita brytningar i stället för att escape:a dem
  till synliga bakstreck. Matematiska radslut hanteras fortsatt av TeX.
- Native editorregression verifierar radgeometri, källvy, kopiering, formatering,
  align-rendering och faktisk PDF-kompilering. Shift+Enter- och backendtesterna
  passerade också, liksom appbygge och isolerad uppstart. Vyn granskades visuellt.

## Overleaf: tomma marginaljusterade rader i export (2026-09-18)

- Användarens export kompilerade, men Overleaf visade fem Underfull hbox-
  meddelanden: fyra explicita radslut före styckeslut och en källnyrad före
  displaymatematik som exporterades till ytterligare ett tvingat radbyte.
- Exporten översätter avslutande radbyte till styckeavstånd och använder
  displaymatematikens egna radgränser. Kopierad källtext ändras inte.
- Den redan importerade Overleaf-filen korrigerades på de fem platserna;
  omkompileringen visar All logs 0, Errors 0, Warnings 0, Info 0.
- Ny backendregression kontrollerar faktisk pdflatex-logg för Underfull,
  bevarade radbyten mitt i text och matematisk radsyntax. Backend- och
  editorregressionerna passerade. Även den korrigerade användarexporten
  kompilerades lokalt utan Underfull/Overfull-meddelanden.
