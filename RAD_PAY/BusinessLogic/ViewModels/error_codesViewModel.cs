using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class error_codesViewModel
    {
        public long id { get; set; }
        public int? value { get; set; }
        public string message_eng { get; set; }
        public string message_rus { get; set; }
        public string message_uzb { get; set; }
        public int? ex_id { get; set; }
    }
}