// MIT. Editable installer copy lives in the embedded SetupStrings.xml table.
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Xml;

internal sealed class SetupText
{
    private sealed class Entry
    {
        internal string English, Chinese, Target, Property;
    }
    private readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
    internal SetupText()
    {
        var document = new XmlDocument { XmlResolver = null };
        var settings = new XmlReaderSettings { DtdProcessing = DtdProcessing.Prohibit, XmlResolver = null };
        using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("setup.strings.xml"))
        using (XmlReader reader = XmlReader.Create(stream, settings)) document.Load(reader);
        foreach (XmlElement element in document.SelectNodes("/strings/string"))
        {
            string key = element.GetAttribute("key");
            if (key.Length == 0 || element["en"] == null || element["zh"] == null)
                throw new InvalidDataException("Each setup string needs a key and en/zh text.");
            entries.Add(key, new Entry { English = element["en"].InnerText, Chinese = element["zh"].InnerText,
                Target = element.GetAttribute("target"), Property = element.GetAttribute("property") });
        }
    }
    internal string Get(string key, bool chinese)
    {
        Entry entry;
        if (!entries.TryGetValue(key, out entry)) throw new InvalidDataException("Missing setup string: " + key);
        return chinese ? entry.Chinese : entry.English;
    }
    internal void Apply(Window window, bool chinese)
    {
        foreach (var pair in entries)
        {
            Entry entry = pair.Value;
            if (entry.Target.Length == 0) continue;
            var control = window.FindName(entry.Target) as FrameworkElement;
            if (control == null) throw new InvalidDataException("Unknown setup text target: " + entry.Target);
            string value = Get(pair.Key, chinese);
            if (entry.Property == "Text" && control is TextBlock) ((TextBlock)control).Text = value;
            else if (entry.Property == "Content" && control is ContentControl) ((ContentControl)control).Content = value;
            else if (entry.Property == "ToolTip")
            {
                control.ToolTip = value;
                AutomationProperties.SetName(control, value);
            }
            else throw new InvalidDataException("Invalid setup text property: " + entry.Property);
        }
    }
}
