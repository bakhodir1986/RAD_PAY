using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class iconViewModel
    {
        public long id { get; set; }
        public string location { get; set; }
        public string path_hash { get; set; }
        public int? kind { get; set; }
        public DateTime? ts { get; set; }
        public long? size { get; set; }
        public string sha1_sum { get; set; }
        public int? b { get; set; }
        public int? c { get; set; }
        public int? d { get; set; }
    }
}